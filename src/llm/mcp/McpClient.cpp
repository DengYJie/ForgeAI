#include "McpClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>

namespace llm::mcp {

    McpClient::McpClient(IMcpTransport* transport, QObject* parent)
        : QObject(parent), m_transport(transport) {
        if (m_transport) {
            connect(m_transport, &IMcpTransport::messageReceived, this, &McpClient::onMessageReceived);
        }
    }

    void McpClient::onMessageReceived(const QJsonObject& message) {
        if (message.contains(QStringLiteral("id")) && !message.value(QStringLiteral("id")).isNull()) {
            const int id = message.value(QStringLiteral("id")).toInt();
            if (m_activeLoops.contains(id)) {
                m_pendingResponses[id] = message;
                auto* loop = m_activeLoops.value(id);
                if (loop && loop->isRunning()) {
                    loop->quit();
                }
            }
        }
    }

    QJsonObject McpClient::sendRequestSync(
        const QString& method,
        const QJsonObject& params,
        int timeoutMs,
        application::ports::CancellationToken cancellationToken
    ) {
        if (cancellationToken.isCanceled()) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{
                {QStringLiteral("code"), -32000},
                {QStringLiteral("message"), QStringLiteral("操作已在调用前取消")}
            }}};
        }

        if (!m_transport || !m_transport->isConnected()) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{
                {QStringLiteral("code"), -32000},
                {QStringLiteral("message"), QStringLiteral("MCP 传输通道未就绪或已断开连接")}
            }}};
        }

        const int requestId = m_nextRequestId.fetch_add(1);
        QJsonObject req{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), requestId},
            {QStringLiteral("method"), method}
        };
        if (!params.isEmpty()) {
            req.insert(QStringLiteral("params"), params);
        }

        QEventLoop loop;
        m_activeLoops.insert(requestId, &loop);

        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);

        QTimer pollTimer;
        connect(&pollTimer, &QTimer::timeout, [&loop, cancellationToken]() {
            if (cancellationToken.isCanceled()) {
                loop.quit();
            }
        });
        pollTimer.start(20);

        if (!m_transport->sendJson(req)) {
            m_activeLoops.remove(requestId);
            return QJsonObject{{QStringLiteral("error"), QJsonObject{
                {QStringLiteral("code"), -32000},
                {QStringLiteral("message"), QStringLiteral("向 MCP 写入请求数据失败")}
            }}};
        }

        loop.exec();
        m_activeLoops.remove(requestId);

        if (cancellationToken.isCanceled()) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{
                {QStringLiteral("code"), -32000},
                {QStringLiteral("message"), QStringLiteral("MCP 请求已取消")}
            }}};
        }

        if (m_pendingResponses.contains(requestId)) {
            return m_pendingResponses.take(requestId);
        }

        return QJsonObject{{QStringLiteral("error"), QJsonObject{
            {QStringLiteral("code"), -32000},
            {QStringLiteral("message"), QStringLiteral("MCP 请求超时 (%1 ms)").arg(timeoutMs)}
        }}};
    }

    bool McpClient::sendNotification(const QString& method, const QJsonObject& params) {
        if (!m_transport || !m_transport->isConnected()) return false;

        QJsonObject req{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("method"), method}
        };
        if (!params.isEmpty()) {
            req.insert(QStringLiteral("params"), params);
        }
        return m_transport->sendJson(req);
    }

    bool McpClient::initialize(int timeoutMs) {
        QJsonObject params{
            {QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05")},
            {QStringLiteral("capabilities"), QJsonObject{
                {QStringLiteral("roots"), QJsonObject{{QStringLiteral("listChanged"), false}}},
                {QStringLiteral("sampling"), QJsonObject{}}
            }},
            {QStringLiteral("clientInfo"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("ForgeAI")},
                {QStringLiteral("version"), QStringLiteral("1.0.0")}
            }}
        };

        const auto resp = sendRequestSync(QStringLiteral("initialize"), params, timeoutMs);
        if (resp.contains(QStringLiteral("result"))) {
            // 发送 initialized 通知完成握手
            sendNotification(QStringLiteral("notifications/initialized"), {});
            return true;
        }
        return false;
    }

    QList<domain::agent::ToolDefinition> McpClient::listTools(int timeoutMs) {
        QList<domain::agent::ToolDefinition> tools;
        const auto resp = sendRequestSync(QStringLiteral("tools/list"), {}, timeoutMs);
        if (!resp.contains(QStringLiteral("result"))) {
            return tools;
        }

        const auto resultObj = resp.value(QStringLiteral("result")).toObject();
        const auto toolsArr = resultObj.value(QStringLiteral("tools")).toArray();

        for (const auto& itemVal : toolsArr) {
            const auto itemObj = itemVal.toObject();
            domain::agent::ToolDefinition def;
            def.name = itemObj.value(QStringLiteral("name")).toString();
            def.description = itemObj.value(QStringLiteral("description")).toString();
            def.parameters = itemObj.value(QStringLiteral("inputSchema")).toObject();
            tools.append(def);
        }

        return tools;
    }

    domain::agent::ToolResult McpClient::callTool(
        const QString& toolCallId,
        const QString& name,
        const QString& argumentsJson,
        int timeoutMs,
        application::ports::CancellationToken cancellationToken
    ) {
        if (thread() && QThread::currentThread() != thread()) {
            domain::agent::ToolResult crossThreadRes{toolCallId, QStringLiteral("跨线程调用失败"), true};
            QMetaObject::invokeMethod(this, [this, toolCallId, name, argumentsJson, timeoutMs, cancellationToken, &crossThreadRes]() {
                crossThreadRes = this->callTool(toolCallId, name, argumentsJson, timeoutMs, cancellationToken);
            }, Qt::BlockingQueuedConnection);
            return crossThreadRes;
        }

        domain::agent::ToolResult result{toolCallId, {}, true};
        const QJsonObject argsObj = QJsonDocument::fromJson(argumentsJson.toUtf8()).object();

        QJsonObject params{
            {QStringLiteral("name"), name},
            {QStringLiteral("arguments"), argsObj}
        };

        const auto resp = sendRequestSync(QStringLiteral("tools/call"), params, timeoutMs, cancellationToken);
        if (resp.contains(QStringLiteral("error"))) {
            const auto errObj = resp.value(QStringLiteral("error")).toObject();
            result.content = errObj.value(QStringLiteral("message")).toString(QStringLiteral("MCP 工具调用错误"));
            result.isError = true;
            return result;
        }

        if (resp.contains(QStringLiteral("result"))) {
            const auto resObj = resp.value(QStringLiteral("result")).toObject();
            result.isError = resObj.value(QStringLiteral("isError")).toBool(false);

            const auto contentArr = resObj.value(QStringLiteral("content")).toArray();
            QStringList texts;
            for (const auto& c : contentArr) {
                const auto cObj = c.toObject();
                if (cObj.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
                    texts.append(cObj.value(QStringLiteral("text")).toString());
                } else {
                    texts.append(QString::fromUtf8(QJsonDocument(cObj).toJson(QJsonDocument::Compact)));
                }
            }
            result.content = texts.join(QLatin1Char('\n'));
            return result;
        }

        result.content = QStringLiteral("MCP 返回异常响应格式");
        result.isError = true;
        return result;
    }

} // namespace llm::mcp
