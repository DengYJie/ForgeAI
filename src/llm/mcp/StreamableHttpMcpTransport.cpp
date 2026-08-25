#include "StreamableHttpMcpTransport.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

namespace llm::mcp {

    StreamableHttpMcpTransport::StreamableHttpMcpTransport(const domain::mcp::McpServerConfig& config, QObject* parent)
        : IMcpTransport(parent), m_config(config) {
    }

    StreamableHttpMcpTransport::~StreamableHttpMcpTransport() {
        close();
    }

    bool StreamableHttpMcpTransport::start() {
        if (m_config.url.isEmpty()) {
            emit errorOccurred(QStringLiteral("HTTP Transport 初始化失败: URL 为空"));
            return false;
        }

        QUrl endpoint(m_config.url);
        if (!endpoint.isValid() || (endpoint.scheme() != QStringLiteral("http") && endpoint.scheme() != QStringLiteral("https"))) {
            emit errorOccurred(QStringLiteral("无效的 HTTP/HTTPS 端点: %1").arg(m_config.url));
            return false;
        }

        if (!m_netManager) {
            m_netManager = std::make_unique<QNetworkAccessManager>();
        }

        m_postEndpoint = endpoint;
        m_sseBuffer.clear();

        QNetworkRequest request{endpoint};
        request.setRawHeader("Accept", "text/event-stream");

        for (auto it = m_config.headers.cbegin(); it != m_config.headers.cend(); ++it) {
            request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
        }

        m_sseReply = m_netManager->get(request);

        connect(m_sseReply, &QNetworkReply::readyRead, this, &StreamableHttpMcpTransport::onSseReadyRead);
        connect(m_sseReply, &QNetworkReply::finished, this, &StreamableHttpMcpTransport::onSseFinished);
        connect(m_sseReply, &QNetworkReply::errorOccurred, this, &StreamableHttpMcpTransport::onSseError);

        m_connected = true;
        return true;
    }

    void StreamableHttpMcpTransport::close() {
        m_connected = false;
        if (m_sseReply) {
            m_sseReply->disconnect(this);
            m_sseReply->abort();
            m_sseReply->deleteLater();
            m_sseReply = nullptr;
        }
        m_netManager.reset();
        m_sseBuffer.clear();
        emit closed();
    }

    bool StreamableHttpMcpTransport::sendJson(const QJsonObject& json) {
        if (!m_connected || !m_netManager || !m_postEndpoint.isValid()) {
            return false;
        }

        QNetworkRequest request{m_postEndpoint};
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "application/json, text/event-stream");

        for (auto it = m_config.headers.cbegin(); it != m_config.headers.cend(); ++it) {
            request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
        }

        const QByteArray payload = QJsonDocument(json).toJson(QJsonDocument::Compact);
        auto* reply = m_netManager->post(request, payload);

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            onPostReplyFinished(reply);
        });

        return true;
    }

    void StreamableHttpMcpTransport::onSseReadyRead() {
        if (!m_sseReply) return;
        m_sseBuffer.append(m_sseReply->readAll());
        processSseBuffer();
    }

    void StreamableHttpMcpTransport::processSseBuffer() {
        while (true) {
            int blockEnd = -1;
            int sepLen = 0;

            int p1 = m_sseBuffer.indexOf("\r\n\r\n");
            int p2 = m_sseBuffer.indexOf("\n\n");

            if (p1 != -1 && (p2 == -1 || p1 <= p2)) {
                blockEnd = p1;
                sepLen = 4;
            } else if (p2 != -1) {
                blockEnd = p2;
                sepLen = 2;
            } else {
                break;
            }

            QByteArray block = m_sseBuffer.left(blockEnd);
            m_sseBuffer.remove(0, blockEnd + sepLen);

            QString blockStr = QString::fromUtf8(block);
            QStringList lines = blockStr.split(QLatin1Char('\n'));

            QString eventType;
            QStringList dataLines;

            for (QString line : lines) {
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
                if (line.isEmpty() || line.startsWith(QLatin1Char(':'))) {
                    continue;
                }
                if (line.startsWith(QStringLiteral("event:"))) {
                    eventType = line.mid(6).trimmed();
                } else if (line.startsWith(QStringLiteral("data:"))) {
                    dataLines.append(line.mid(5).trimmed());
                }
            }

            const QString fullData = dataLines.join(QLatin1Char('\n'));
            if (!fullData.isEmpty() || !eventType.isEmpty()) {
                handleSseEvent(eventType, fullData);
            }
        }
    }

    void StreamableHttpMcpTransport::handleSseEvent(const QString& eventName, const QString& eventData) {
        if (eventName.compare(QStringLiteral("endpoint"), Qt::CaseInsensitive) == 0) {
            QUrl baseUrl(m_config.url);
            m_postEndpoint = baseUrl.resolved(QUrl(eventData.trimmed()));
            m_connected = true;
        } else if (eventName.isEmpty() || eventName.compare(QStringLiteral("message"), Qt::CaseInsensitive) == 0) {
            m_connected = true;
            QJsonParseError parseError;
            const auto doc = QJsonDocument::fromJson(eventData.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                emit messageReceived(doc.object());
            } else if (!eventData.trimmed().isEmpty()) {
                emit errorOccurred(QStringLiteral("SSE 消息格式解析失败: %1").arg(parseError.errorString()));
            }
        }
    }

    void StreamableHttpMcpTransport::onSseFinished() {
        if (m_connected) {
            m_connected = false;
            emit closed();
        }
    }

    void StreamableHttpMcpTransport::onSseError(QNetworkReply::NetworkError code) {
        if (code != QNetworkReply::NoError && code != QNetworkReply::OperationCanceledError) {
            if (m_sseReply) {
                emit errorOccurred(QStringLiteral("SSE 连接错误: %1").arg(m_sseReply->errorString()));
            }
        }
    }

    void StreamableHttpMcpTransport::onPostReplyFinished(QNetworkReply* reply) {
        if (!reply) return;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(QStringLiteral("HTTP MCP POST 请求失败: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray responseData = reply->readAll();
        if (!responseData.trimmed().isEmpty()) {
            QJsonParseError parseError;
            const auto doc = QJsonDocument::fromJson(responseData, &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                emit messageReceived(doc.object());
            }
        }
    }

    bool StreamableHttpMcpTransport::isConnected() const {
        return m_connected;
    }

} // namespace llm::mcp
