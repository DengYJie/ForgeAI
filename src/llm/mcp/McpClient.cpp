#include "McpClient.h"
#include "McpToolOperation.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"
#include "core/logging/SensitiveDataFilter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>

namespace llm::mcp {

    McpClient::McpClient(IMcpTransport* transport, QObject* parent)
        : QObject(parent), m_transport(transport) {
        if (m_transport) {
            connect(m_transport, &IMcpTransport::messageReceived, this, &McpClient::onMessageReceived);
            connect(m_transport, &IMcpTransport::closed, this, [this]() {
                auto pendingMap = m_pendingAsyncRequests;
                m_pendingAsyncRequests.clear();
                for (auto req : pendingMap) {
                    if (req.timeoutTimer) {
                        req.timeoutTimer->stop();
                        req.timeoutTimer->deleteLater();
                    }
                    if (req.callback) {
                        req.callback({}, true, QStringLiteral("MCP 服务未连接或已断开连接"));
                    }
                }
            });
        }
    }

    McpClient::~McpClient() {
        for (auto& req : m_pendingAsyncRequests) {
            if (req.timeoutTimer) {
                req.timeoutTimer->stop();
                delete req.timeoutTimer;
                req.timeoutTimer = nullptr;
            }
        }
        m_pendingAsyncRequests.clear();
    }

    void McpClient::onMessageReceived(const QJsonObject& message) {
        if (message.contains(QStringLiteral("id")) && !message.value(QStringLiteral("id")).isNull()) {
            const int id = message.value(QStringLiteral("id")).toInt();
            if (m_pendingAsyncRequests.contains(id)) {
                auto req = m_pendingAsyncRequests.take(id);
                if (req.timeoutTimer) {
                    req.timeoutTimer->stop();
                    req.timeoutTimer->deleteLater();
                }
                if (req.callback) {
                    req.callback(message, false, QString());
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
            core::logging::LoggingService::instance().info(core::logging::Category::McpProtocol, QStringLiteral("MCP 请求在调用前已被取消"), {
                {QStringLiteral("method"), method}
            });
            return QJsonObject{{QStringLiteral("error"), QJsonObject{
                {QStringLiteral("code"), -32000},
                {QStringLiteral("message"), QStringLiteral("操作已在调用前取消")}
            }}};
        }

        if (!m_transport || !m_transport->isConnected()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP 传输通道未就绪或已断开连接"), {
                {QStringLiteral("method"), method}
            });
            return QJsonObject{{QStringLiteral("error"), QJsonObject{
                {QStringLiteral("code"), -32000},
                {QStringLiteral("message"), QStringLiteral("MCP 服务未连接或已断开连接")}
            }}};
        }

        QJsonObject resultObj;
        bool done = false;
        QEventLoop loop;
        const int requestId = sendRequestAsync(
            method,
            params,
            timeoutMs,
            [&resultObj, &loop, &done](const QJsonObject& response, bool isError, const QString& errorMessage) {
                if (isError) {
                    resultObj = QJsonObject{{QStringLiteral("error"), QJsonObject{
                        {QStringLiteral("code"), -32000},
                        {QStringLiteral("message"), errorMessage}
                    }}};
                } else {
                    resultObj = response;
                }
                done = true;
                if (loop.isRunning()) {
                    loop.quit();
                }
            }
        );

        if (requestId < 0) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{
                {QStringLiteral("code"), -32000},
                {QStringLiteral("message"), QStringLiteral("发送 MCP 请求失败")}
            }}};
        }

        if (!done) {
            loop.exec();
        }
        return resultObj;
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

    bool McpClient::isProtocolVersionSupported(const QString& version) const {
        if (version.isEmpty()) return true;
        if (version == QStringLiteral("2024-11-05") || version == QStringLiteral("latest")) return true;
        if (version.startsWith(QStringLiteral("2024-")) || version.startsWith(QStringLiteral("0."))) return true;
        return false;
    }

    bool McpClient::initialize(int timeoutMs) {
        QJsonObject params{
            {QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05")},
            {QStringLiteral("capabilities"), QJsonObject{
                {QStringLiteral("roots"), QJsonObject{{QStringLiteral("listChanged"), false}}},
                {QStringLiteral("sampling"), QJsonObject{}},
                {QStringLiteral("resources"), QJsonObject{}},
                {QStringLiteral("prompts"), QJsonObject{}}
            }},
            {QStringLiteral("clientInfo"), QJsonObject{
                {QStringLiteral("name"), QStringLiteral("ForgeAI")},
                {QStringLiteral("version"), QStringLiteral("1.0.0")}
            }}
        };

        const auto resp = sendRequestSync(QStringLiteral("initialize"), params, timeoutMs);
        if (resp.contains(QStringLiteral("result"))) {
            const auto resultObj = resp.value(QStringLiteral("result")).toObject();
            const QString serverProtocolVersion = resultObj.value(QStringLiteral("protocolVersion")).toString();
            
            // 显式校验协议版本兼容性
            if (!serverProtocolVersion.isEmpty() && !isProtocolVersionSupported(serverProtocolVersion)) {
                m_lastError = QStringLiteral("MCP 协议版本不兼容: 服务端返回 '%1', 客户端要求 '2024-11-05' 系列").arg(serverProtocolVersion);
                return false;
            }

            // 发送 initialized 通知完成握手
            sendNotification(QStringLiteral("notifications/initialized"), {});
            return true;
        }

        if (resp.contains(QStringLiteral("error"))) {
            m_lastError = resp.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
        } else {
            m_lastError = QStringLiteral("MCP 握手未返回 result 或响应格式非法");
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

        for (const auto& toolVal : toolsArr) {
            if (!toolVal.isObject()) continue;
            const auto toolObj = toolVal.toObject();

            domain::agent::ToolDefinition def;
            def.name = toolObj.value(QStringLiteral("name")).toString();
            def.description = toolObj.value(QStringLiteral("description")).toString();

            if (toolObj.contains(QStringLiteral("inputSchema"))) {
                def.parameters = toolObj.value(QStringLiteral("inputSchema")).toObject();
            }

            if (!def.name.isEmpty()) {
                tools.append(def);
            }
        }

        return tools;
    }

    QList<domain::mcp::McpResource> McpClient::listResources(int timeoutMs) {
        QList<domain::mcp::McpResource> resources;
        const auto resp = sendRequestSync(QStringLiteral("resources/list"), {}, timeoutMs);
        if (!resp.contains(QStringLiteral("result"))) {
            return resources;
        }

        const auto resultObj = resp.value(QStringLiteral("result")).toObject();
        const auto resArr = resultObj.value(QStringLiteral("resources")).toArray();

        for (const auto& resVal : resArr) {
            if (!resVal.isObject()) continue;
            const auto resObj = resVal.toObject();

            domain::mcp::McpResource res;
            res.uri = resObj.value(QStringLiteral("uri")).toString();
            res.name = resObj.value(QStringLiteral("name")).toString();
            res.description = resObj.value(QStringLiteral("description")).toString();
            res.mimeType = resObj.value(QStringLiteral("mimeType")).toString();

            if (!res.uri.isEmpty()) {
                resources.append(res);
            }
        }

        return resources;
    }

    std::optional<domain::mcp::McpResourceContent> McpClient::readResource(const QString& uri, int timeoutMs) {
        QJsonObject params{{QStringLiteral("uri"), uri}};
        const auto resp = sendRequestSync(QStringLiteral("resources/read"), params, timeoutMs);
        if (!resp.contains(QStringLiteral("result"))) {
            return std::nullopt;
        }

        const auto resultObj = resp.value(QStringLiteral("result")).toObject();
        const auto contentsArr = resultObj.value(QStringLiteral("contents")).toArray();
        if (contentsArr.isEmpty() || !contentsArr.first().isObject()) {
            return std::nullopt;
        }

        const auto contentObj = contentsArr.first().toObject();
        domain::mcp::McpResourceContent content;
        content.uri = contentObj.value(QStringLiteral("uri")).toString(uri);
        content.mimeType = contentObj.value(QStringLiteral("mimeType")).toString();
        content.text = contentObj.value(QStringLiteral("text")).toString();
        if (contentObj.contains(QStringLiteral("blob"))) {
            content.blob = QByteArray::fromBase64(contentObj.value(QStringLiteral("blob")).toString().toUtf8());
        }

        return content;
    }

    QList<domain::mcp::McpPrompt> McpClient::listPrompts(int timeoutMs) {
        QList<domain::mcp::McpPrompt> prompts;
        const auto resp = sendRequestSync(QStringLiteral("prompts/list"), {}, timeoutMs);
        if (!resp.contains(QStringLiteral("result"))) {
            return prompts;
        }

        const auto resultObj = resp.value(QStringLiteral("result")).toObject();
        const auto promptsArr = resultObj.value(QStringLiteral("prompts")).toArray();

        for (const auto& pVal : promptsArr) {
            if (!pVal.isObject()) continue;
            const auto pObj = pVal.toObject();

            domain::mcp::McpPrompt prompt;
            prompt.name = pObj.value(QStringLiteral("name")).toString();
            prompt.description = pObj.value(QStringLiteral("description")).toString();

            const auto argsArr = pObj.value(QStringLiteral("arguments")).toArray();
            for (const auto& aVal : argsArr) {
                if (!aVal.isObject()) continue;
                const auto aObj = aVal.toObject();
                domain::mcp::McpPromptArgument arg;
                arg.name = aObj.value(QStringLiteral("name")).toString();
                arg.description = aObj.value(QStringLiteral("description")).toString();
                arg.required = aObj.value(QStringLiteral("required")).toBool(false);
                prompt.arguments.append(arg);
            }

            if (!prompt.name.isEmpty()) {
                prompts.append(prompt);
            }
        }

        return prompts;
    }

    QList<domain::mcp::McpPromptMessage> McpClient::getPrompt(
        const QString& name,
        const QJsonObject& arguments,
        int timeoutMs
    ) {
        QList<domain::mcp::McpPromptMessage> messages;
        QJsonObject params{{QStringLiteral("name"), name}};
        if (!arguments.isEmpty()) {
            params.insert(QStringLiteral("arguments"), arguments);
        }

        const auto resp = sendRequestSync(QStringLiteral("prompts/get"), params, timeoutMs);
        if (!resp.contains(QStringLiteral("result"))) {
            return messages;
        }

        const auto resultObj = resp.value(QStringLiteral("result")).toObject();
        const auto messagesArr = resultObj.value(QStringLiteral("messages")).toArray();

        for (const auto& mVal : messagesArr) {
            if (!mVal.isObject()) continue;
            const auto mObj = mVal.toObject();

            domain::mcp::McpPromptMessage msg;
            msg.role = mObj.value(QStringLiteral("role")).toString();

            const auto contentVal = mObj.value(QStringLiteral("content"));
            if (contentVal.isObject()) {
                msg.content = contentVal.toObject().value(QStringLiteral("text")).toString();
            } else {
                msg.content = contentVal.toString();
            }

            messages.append(msg);
        }

        return messages;
    }

    domain::agent::ToolResult McpClient::callTool(
        const QString& toolCallId,
        const QString& name,
        const QString& argumentsJson,
        int timeoutMs,
        application::ports::CancellationToken cancellationToken
    ) {
        core::logging::LoggingService::instance().debug(core::logging::Category::McpProtocol, QStringLiteral("调用 MCP 工具"), {
            {QStringLiteral("toolName"), name},
            {QStringLiteral("callId"), toolCallId},
            {QStringLiteral("argKeys"), core::logging::SensitiveDataFilter::extractArgKeys(argumentsJson)}
        });

        QJsonObject argsObj;
        if (!argumentsJson.trimmed().isEmpty()) {
            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(argumentsJson.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                argsObj = doc.object();
            }
        }

        QJsonObject params{
            {QStringLiteral("name"), name},
            {QStringLiteral("arguments"), argsObj}
        };

        const auto resp = sendRequestSync(QStringLiteral("tools/call"), params, timeoutMs, cancellationToken);

        if (resp.contains(QStringLiteral("error"))) {
            const auto errObj = resp.value(QStringLiteral("error")).toObject();
            const QString errMsg = errObj.value(QStringLiteral("message")).toString();

            core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP 工具调用返回错误"), {
                {QStringLiteral("toolName"), name},
                {QStringLiteral("callId"), toolCallId},
                {QStringLiteral("error"), errMsg}
            });

            return domain::agent::ToolResult{
                toolCallId,
                errMsg,
                true
            };
        }

        if (!resp.contains(QStringLiteral("result"))) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP 工具未返回有效结果"), {
                {QStringLiteral("toolName"), name},
                {QStringLiteral("callId"), toolCallId}
            });

            return domain::agent::ToolResult{
                toolCallId,
                QStringLiteral("MCP 服务未返回有效结果"),
                true
            };
        }

        const auto resultObj = resp.value(QStringLiteral("result")).toObject();
        const bool isError = resultObj.value(QStringLiteral("isError")).toBool(false);
        const auto contentArr = resultObj.value(QStringLiteral("content")).toArray();

        QStringList textOutputs;
        for (const auto& itemVal : contentArr) {
            if (itemVal.isObject()) {
                const auto itemObj = itemVal.toObject();
                const QString type = itemObj.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("text")) {
                    textOutputs.append(itemObj.value(QStringLiteral("text")).toString());
                } else if (type == QStringLiteral("image")) {
                    textOutputs.append(QStringLiteral("[Image content: %1]").arg(itemObj.value(QStringLiteral("mimeType")).toString()));
                } else if (type == QStringLiteral("resource")) {
                    textOutputs.append(QStringLiteral("[Embedded resource: %1]").arg(itemObj.value(QStringLiteral("resource")).toObject().value(QStringLiteral("uri")).toString()));
                }
            } else if (itemVal.isString()) {
                textOutputs.append(itemVal.toString());
            }
        }

        QString finalContent = textOutputs.join(QStringLiteral("\n"));
        if (finalContent.isEmpty()) {
            finalContent = QString::fromUtf8(QJsonDocument(resultObj).toJson(QJsonDocument::Compact));
        }

        core::logging::LoggingService::instance().debug(core::logging::Category::McpProtocol, QStringLiteral("MCP 工具调用完成"), {
            {QStringLiteral("toolName"), name},
            {QStringLiteral("callId"), toolCallId},
            {QStringLiteral("isError"), isError ? QStringLiteral("true") : QStringLiteral("false")},
            {QStringLiteral("contentLength"), QString::number(finalContent.length())}
        });

        return domain::agent::ToolResult{
            toolCallId,
            finalContent,
            isError
        };
    }

    int McpClient::sendRequestAsync(
        const QString& method,
        const QJsonObject& params,
        int timeoutMs,
        McpResponseCallback callback
    ) {
        if (!m_transport || !m_transport->isConnected()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP 传输通道未就绪或已断开连接"), {
                {QStringLiteral("method"), method}
            });
            return -1;
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

        core::logging::LoggingService::instance().debug(core::logging::Category::McpProtocol, QStringLiteral("发送 MCP 异步请求"), {
            {QStringLiteral("method"), method},
            {QStringLiteral("requestId"), QString::number(requestId)}
        });

        PendingAsyncRequest pending;
        pending.id = requestId;
        pending.method = method;
        pending.callback = std::move(callback);

        auto* timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, requestId, method]() {
            if (m_pendingAsyncRequests.contains(requestId)) {
                auto req = m_pendingAsyncRequests.take(requestId);
                if (req.timeoutTimer) {
                    req.timeoutTimer->deleteLater();
                }
                core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("MCP 异步请求超时"), {
                    {QStringLiteral("method"), method},
                    {QStringLiteral("requestId"), QString::number(requestId)}
                });
                if (req.callback) {
                    req.callback({}, true, QStringLiteral("服务响应超时，请稍后重试。"));
                }
            }
        });
        timer->start(timeoutMs);
        pending.timeoutTimer = timer;

        m_pendingAsyncRequests.insert(requestId, pending);

        if (!m_transport->sendJson(req)) {
            if (m_pendingAsyncRequests.contains(requestId)) {
                auto failedReq = m_pendingAsyncRequests.take(requestId);
                if (failedReq.timeoutTimer) {
                    failedReq.timeoutTimer->stop();
                    failedReq.timeoutTimer->deleteLater();
                }
                core::logging::LoggingService::instance().warn(core::logging::Category::McpProtocol, QStringLiteral("向 MCP 写入异步请求数据失败"), {
                    {QStringLiteral("method"), method},
                    {QStringLiteral("requestId"), QString::number(requestId)}
                });
                if (failedReq.callback) {
                    failedReq.callback({}, true, QStringLiteral("发送 MCP 请求失败"));
                }
            }
            return -1;
        }

        return requestId;
    }

    void McpClient::cancelRequest(int requestId) {
        if (m_pendingAsyncRequests.contains(requestId)) {
            auto req = m_pendingAsyncRequests.take(requestId);
            if (req.timeoutTimer) {
                req.timeoutTimer->stop();
                req.timeoutTimer->deleteLater();
            }
            core::logging::LoggingService::instance().info(core::logging::Category::McpProtocol, QStringLiteral("取消 MCP 待处理异步请求"), {
                {QStringLiteral("method"), req.method},
                {QStringLiteral("requestId"), QString::number(requestId)}
            });
            if (req.callback) {
                req.callback({}, true, QStringLiteral("操作已取消"));
            }
        }
    }

    std::unique_ptr<application::ports::IToolOperation> McpClient::callToolAsync(
        const QString& toolCallId,
        const QString& name,
        const QString& argumentsJson,
        int timeoutMs,
        application::ports::CancellationToken cancellationToken
    ) {
        QJsonObject argsObj;
        if (!argumentsJson.trimmed().isEmpty()) {
            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(argumentsJson.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                argsObj = doc.object();
            }
        }

        QJsonObject params{
            {QStringLiteral("name"), name},
            {QStringLiteral("arguments"), argsObj}
        };

        return std::make_unique<McpToolOperation>(
            this,
            toolCallId,
            name,
            params,
            timeoutMs,
            cancellationToken
        );
    }

} // namespace llm::mcp
