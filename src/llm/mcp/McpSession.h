#pragma once

#include <QObject>
#include <memory>
#include "McpServerConfig.h"
#include "StdioMcpTransport.h"
#include "McpClient.h"

namespace llm::mcp {

    enum class McpSessionState {
        Disconnected,
        Connecting,
        Connected,
        Error
    };

    /**
     * @brief 单个 MCP 服务的运行会话（整合配置、传输通道与 JSON-RPC 客户端）
     */
    class McpSession : public QObject {
        Q_OBJECT
    public:
        explicit McpSession(const McpServerConfig& config, QObject* parent = nullptr);
        ~McpSession() override;

        bool start();
        void stop();

        McpSessionState state() const;
        McpServerConfig config() const;
        McpClient* client() const;
        QString lastError() const;

        QList<domain::agent::ToolDefinition> tools() const;

    Q_SIGNALS:
        void stateChanged(McpSessionState state);
        void errorOccurred(const QString& error);
        void toolsUpdated(const QList<domain::agent::ToolDefinition>& tools);

    private Q_SLOTS:
        void onTransportError(const QString& error);
        void onTransportClosed();

    private:
        void setState(McpSessionState state);

        McpServerConfig m_config;
        std::unique_ptr<StdioMcpTransport> m_transport;
        std::unique_ptr<McpClient> m_client;
        McpSessionState m_state = McpSessionState::Disconnected;
        QString m_lastError;
        QList<domain::agent::ToolDefinition> m_cachedTools;
    };

} // namespace llm::mcp
