#include "StreamableHttpMcpTransport.h"
#include <QJsonDocument>
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

        m_connected = true;
        return true;
    }

    void StreamableHttpMcpTransport::close() {
        m_connected = false;
        m_netManager.reset();
        emit closed();
    }

    bool StreamableHttpMcpTransport::sendJson(const QJsonObject& json) {
        if (!m_connected || !m_netManager) {
            return false;
        }

        QNetworkRequest request{QUrl(m_config.url)};
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "application/json, text/event-stream");

        for (auto it = m_config.headers.cbegin(); it != m_config.headers.cend(); ++it) {
            request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
        }

        const QByteArray payload = QJsonDocument(json).toJson(QJsonDocument::Compact);
        auto* reply = m_netManager->post(request, payload);

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            onReplyFinished(reply);
        });

        return true;
    }

    void StreamableHttpMcpTransport::onReplyFinished(QNetworkReply* reply) {
        if (!reply) return;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(QStringLiteral("HTTP MCP 请求失败: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            emit messageReceived(doc.object());
        } else {
            emit errorOccurred(QStringLiteral("HTTP MCP 响应不是有效 JSON: %1").arg(parseError.errorString()));
        }
    }

    bool StreamableHttpMcpTransport::isConnected() const {
        return m_connected;
    }

} // namespace llm::mcp
