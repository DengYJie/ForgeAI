#pragma once

#include "IMcpTransport.h"
#include "domain/mcp/McpServerConfig.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QByteArray>
#include <memory>

namespace llm::mcp {

    /**
     * @brief 基于 HTTP / SSE (Server-Sent Events) 流式通信的 MCP 传输通道
     */
    class StreamableHttpMcpTransport final : public IMcpTransport {
        Q_OBJECT
    public:
        explicit StreamableHttpMcpTransport(const domain::mcp::McpServerConfig& config, QObject* parent = nullptr);
        ~StreamableHttpMcpTransport() override;

        bool start() override;
        void close() override;
        bool sendJson(const QJsonObject& json) override;
        bool isConnected() const override;

        QUrl postEndpoint() const { return m_postEndpoint; }

    private Q_SLOTS:
        void onSseReadyRead();
        void onSseFinished();
        void onSseError(QNetworkReply::NetworkError code);
        void onPostReplyFinished(QNetworkReply* reply);

    private:
        void processSseBuffer();
        void handleSseEvent(const QString& eventName, const QString& eventData);

        domain::mcp::McpServerConfig m_config;
        std::unique_ptr<QNetworkAccessManager> m_netManager;
        QNetworkReply* m_sseReply = nullptr;
        bool m_connected = false;
        QUrl m_postEndpoint;
        QByteArray m_sseBuffer;
    };

} // namespace llm::mcp
