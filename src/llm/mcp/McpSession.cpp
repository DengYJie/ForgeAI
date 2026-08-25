#include "McpSession.h"
#include "StdioMcpTransport.h"

namespace llm::mcp {

    McpSession::McpSession(
        const domain::mcp::McpServerConfig& config,
        std::unique_ptr<IMcpTransport> transport,
        QObject* parent
    ) : QObject(parent), m_config(config), m_transport(std::move(transport)) {
    }

    McpSession::~McpSession() {
        stop();
    }

    domain::mcp::McpConnectionState McpSession::state() const {
        return m_state;
    }

    domain::mcp::McpServerConfig McpSession::config() const {
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

    void McpSession::setState(domain::mcp::McpConnectionState state) {
        if (m_state != state) {
            m_state = state;
            emit stateChanged(m_state);
        }
    }

    bool McpSession::start() {
        if (!m_config.isEnabled()) return false;
        if (m_state == domain::mcp::McpConnectionState::Ready) return true;

        if (m_state != domain::mcp::McpConnectionState::Stopped) {
            stop();
        }
        setState(domain::mcp::McpConnectionState::Starting);

        if (!m_transport) {
            McpTransportFactory factory;
            m_transport = factory.create(m_config);
        }

        if (!m_transport) {
            m_lastError = QStringLiteral("无法为服务 '%1' 创建传输通道").arg(m_config.id);
            setState(domain::mcp::McpConnectionState::Failed);
            emit errorOccurred(m_lastError);
            return false;
        }

        m_transport->disconnect(this);
        connect(m_transport.get(), &IMcpTransport::errorOccurred, this, &McpSession::onTransportError);
        connect(m_transport.get(), &IMcpTransport::closed, this, &McpSession::onTransportClosed);

        if (!m_transport->start()) {
            m_lastError = QStringLiteral("无法启动 MCP 进程/通道: %1").arg(m_config.command.isEmpty() ? m_config.url : m_config.command);
            setState(domain::mcp::McpConnectionState::Failed);
            emit errorOccurred(m_lastError);
            return false;
        }

        setState(domain::mcp::McpConnectionState::Initializing);
        m_client = std::make_unique<McpClient>(m_transport.get());
        if (!m_client->initialize()) {
            m_lastError = m_client->lastError().isEmpty() ? QStringLiteral("MCP 握手初始化失败") : m_client->lastError();
            setState(domain::mcp::McpConnectionState::Failed);
            emit errorOccurred(m_lastError);
            return false;
        }

        m_cachedTools = m_client->listTools();
        setState(domain::mcp::McpConnectionState::Ready);
        emit toolsUpdated(m_cachedTools);
        return true;
    }

    void McpSession::stop() {
        setState(domain::mcp::McpConnectionState::Stopping);
        if (m_transport) {
            m_transport->disconnect(this);
            m_transport->close();
        }
        m_client.reset();
        m_cachedTools.clear();
        setState(domain::mcp::McpConnectionState::Stopped);
    }

    void McpSession::onTransportError(const QString& error) {
        m_lastError = error;
        setState(domain::mcp::McpConnectionState::Failed);
        emit errorOccurred(m_lastError);
    }

    void McpSession::onTransportClosed() {
        if (m_state != domain::mcp::McpConnectionState::Stopping &&
            m_state != domain::mcp::McpConnectionState::Stopped &&
            m_state != domain::mcp::McpConnectionState::Failed) {
            setState(domain::mcp::McpConnectionState::Stopped);
        }
    }

} // namespace llm::mcp
