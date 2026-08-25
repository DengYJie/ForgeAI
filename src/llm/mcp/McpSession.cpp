#include "McpSession.h"

namespace llm::mcp {

    McpSession::McpSession(const McpServerConfig& config, QObject* parent)
        : QObject(parent), m_config(config) {
    }

    McpSession::~McpSession() {
        stop();
    }

    McpSessionState McpSession::state() const {
        return m_state;
    }

    McpServerConfig McpSession::config() const {
        return m_config;
    }

    McpClient* McpSession::client() const {
        return m_client.get();
    }

    QString McpSession::lastError() const {
        return m_lastError;
    }

    QList<domain::agent::ToolDefinition> McpSession::tools() const {
        return m_cachedTools;
    }

    void McpSession::setState(McpSessionState state) {
        if (m_state != state) {
            m_state = state;
            emit stateChanged(m_state);
        }
    }

    bool McpSession::start() {
        if (m_config.disabled) return false;
        if (m_state == McpSessionState::Connected) return true;

        stop();
        setState(McpSessionState::Connecting);

        m_transport = std::make_unique<StdioMcpTransport>(m_config);
        connect(m_transport.get(), &IMcpTransport::errorOccurred, this, &McpSession::onTransportError);
        connect(m_transport.get(), &IMcpTransport::closed, this, &McpSession::onTransportClosed);

        if (!m_transport->start()) {
            m_lastError = QStringLiteral("无法启动 MCP 进程: %1").arg(m_config.command);
            setState(McpSessionState::Error);
            emit errorOccurred(m_lastError);
            return false;
        }

        m_client = std::make_unique<McpClient>(m_transport.get());
        if (!m_client->initialize()) {
            m_lastError = QStringLiteral("MCP 握手初始化失败");
            setState(McpSessionState::Error);
            emit errorOccurred(m_lastError);
            return false;
        }

        m_cachedTools = m_client->listTools();
        setState(McpSessionState::Connected);
        emit toolsUpdated(m_cachedTools);
        return true;
    }

    void McpSession::stop() {
        if (m_transport) {
            m_transport->disconnect(this);
            m_transport->close();
            m_transport.reset();
        }
        m_client.reset();
        m_cachedTools.clear();
        setState(McpSessionState::Disconnected);
    }

    void McpSession::onTransportError(const QString& error) {
        m_lastError = error;
        setState(McpSessionState::Error);
        emit errorOccurred(error);
    }

    void McpSession::onTransportClosed() {
        if (m_state != McpSessionState::Disconnected) {
            setState(McpSessionState::Disconnected);
        }
    }

} // namespace llm::mcp
