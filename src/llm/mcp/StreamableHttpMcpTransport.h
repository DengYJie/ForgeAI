#pragma once

#include "IMcpTransport.h"
#include "domain/mcp/McpServerConfig.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <memory>

namespace llm::mcp {

    /**
     * @brief 基于 HTTP / SSE 流式通信的 MCP 传输通道
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

    private Q_SLOTS:
        void onReplyFinished(QNetworkReply* reply);

    private:
        domain::mcp::McpServerConfig m_config;
        std::unique_ptr<QNetworkAccessManager> m_netManager;
        bool m_connected = false;
    };

} // namespace llm::mcp
