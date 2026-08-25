#include "McpSession.h"
#include "StdioMcpTransport.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

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

        core::logging::LoggingService::instance().info(core::logging::Category::McpSession, QStringLiteral("MCP 会话启动中"), {
            {QStringLiteral("serverId"), m_config.id}
        });

        if (!m_transport) {
            McpTransportFactory factory;
            m_transport = factory.create(m_config);
        }

        if (!m_transport) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpSession, QStringLiteral("无法创建传输通道"), {
                {QStringLiteral("serverId"), m_config.id}
            });
            m_lastError = QStringLiteral("MCP 服务启动失败，请检查配置。");
            setState(domain::mcp::McpConnectionState::Failed);
            emit errorOccurred(m_lastError);
            return false;
        }

        m_transport->disconnect(this);
        connect(m_transport.get(), &IMcpTransport::errorOccurred, this, &McpSession::onTransportError);
        connect(m_transport.get(), &IMcpTransport::closed, this, &McpSession::onTransportClosed);

        if (!m_transport->start()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpSession, QStringLiteral("启动底层传输通道失败"), {
                {QStringLiteral("serverId"), m_config.id}
            });
            m_lastError = QStringLiteral("MCP 服务启动失败，请检查配置。");
            setState(domain::mcp::McpConnectionState::Failed);
            emit errorOccurred(m_lastError);
            return false;
        }

        setState(domain::mcp::McpConnectionState::Initializing);
        m_client = std::make_unique<McpClient>(m_transport.get());
        if (!m_client->initialize()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpSession, QStringLiteral("MCP 握手初始化失败"), {
                {QStringLiteral("serverId"), m_config.id},
                {QStringLiteral("detail"), m_client->lastError()}
            });
            m_lastError = m_client->lastError().isEmpty() ? QStringLiteral("MCP 服务版本不兼容。") : m_client->lastError();
            setState(domain::mcp::McpConnectionState::Failed);
            emit errorOccurred(m_lastError);
            return false;
        }

        m_cachedTools = m_client->listTools();
        setState(domain::mcp::McpConnectionState::Ready);

        core::logging::LoggingService::instance().info(core::logging::Category::McpSession, QStringLiteral("MCP 会话已就绪"), {
            {QStringLiteral("serverId"), m_config.id},
            {QStringLiteral("toolsCount"), QString::number(m_cachedTools.size())}
        });

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

        core::logging::LoggingService::instance().info(core::logging::Category::McpSession, QStringLiteral("MCP 会话已停止"), {
            {QStringLiteral("serverId"), m_config.id}
        });
    }

    void McpSession::onTransportError(const QString& error) {
        core::logging::LoggingService::instance().warn(core::logging::Category::McpSession, QStringLiteral("传输通道发生异常"), {
            {QStringLiteral("serverId"), m_config.id},
            {QStringLiteral("error"), error}
        });
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
