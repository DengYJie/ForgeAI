#pragma once

#include <QObject>
#include <memory>
#include "domain/mcp/McpServerConfig.h"
#include "domain/mcp/McpConnectionState.h"
#include "IMcpTransport.h"
#include "McpTransportFactory.h"
#include "McpClient.h"

namespace llm::mcp {

    using McpSessionState = domain::mcp::McpConnectionState;

    /**
     * @brief 单个 MCP 服务的运行会话（整合配置、抽象传输通道与 JSON-RPC 客户端）
     */
    class McpSession : public QObject {
        Q_OBJECT
    public:
        explicit McpSession(
            const domain::mcp::McpServerConfig& config,
            std::unique_ptr<IMcpTransport> transport = nullptr,
            QObject* parent = nullptr
        );
        ~McpSession() override;

        bool start();
        void stop();

        domain::mcp::McpConnectionState state() const;
        domain::mcp::McpServerConfig config() const;
        McpClient* client() const;
        QString lastError() const;

        QList<domain::agent::ToolDefinition> tools() const;

    Q_SIGNALS:
        void stateChanged(domain::mcp::McpConnectionState state);
        void errorOccurred(const QString& error);
        void toolsUpdated(const QList<domain::agent::ToolDefinition>& tools);

    private Q_SLOTS:
        void onTransportError(const QString& error);
        void onTransportClosed();

    private:
        void setState(domain::mcp::McpConnectionState state);

        domain::mcp::McpServerConfig m_config;
        std::unique_ptr<IMcpTransport> m_transport;
        std::unique_ptr<McpClient> m_client;
        domain::mcp::McpConnectionState m_state = domain::mcp::McpConnectionState::Stopped;
        QString m_lastError;
        QList<domain::agent::ToolDefinition> m_cachedTools;
    };

} // namespace llm::mcp
