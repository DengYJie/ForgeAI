#include "McpRuntime.h"
#include "McpResourceProvider.h"
#include "McpPromptProvider.h"
#include <QFileInfo>
#include <QDir>

namespace llm::mcp {

    McpRuntime::McpRuntime(
        McpServerRegistry* registry,
        std::unique_ptr<McpTransportFactory> transportFactory,
        QObject* parent
    ) : QObject(parent),
        m_registry(registry),
        m_transportFactory(std::move(transportFactory)) {
        if (!m_transportFactory) {
            m_transportFactory = std::make_unique<McpTransportFactory>();
        }
        m_toolProvider = std::make_shared<McpToolProvider>(this);
        m_resourceProvider = std::make_shared<McpResourceProvider>(this);
        m_promptProvider = std::make_shared<McpPromptProvider>(this);

        if (m_registry) {
            setRegistry(m_registry);
        }
    }

    McpRuntime::~McpRuntime() {
        stopAll();
    }

    void McpRuntime::setTrustPolicy(const domain::mcp::McpServerTrustPolicy& policy) {
        m_trustPolicy = policy;
    }

    domain::mcp::McpServerTrustPolicy& McpRuntime::trustPolicy() {
        return m_trustPolicy;
    }

    const domain::mcp::McpServerTrustPolicy& McpRuntime::trustPolicy() const {
        return m_trustPolicy;
    }

    void McpRuntime::setRegistry(McpServerRegistry* registry) {
        if (m_registry) {
            disconnect(m_registry, nullptr, this, nullptr);
        }
        m_registry = registry;
        if (m_registry) {
            connect(m_registry, &McpServerRegistry::serverRegistered, this, &McpRuntime::onServerRegistered);
            connect(m_registry, &McpServerRegistry::serverUnregistered, this, &McpRuntime::onServerUnregistered);
            for (const auto& cfg : m_registry->servers()) {
                initSession(cfg);
            }
        }
    }

    McpServerRegistry* McpRuntime::registry() const {
        return m_registry;
    }

    void McpRuntime::initSession(const domain::mcp::McpServerConfig& config) {
        const QString key = config.id.isEmpty() ? config.name : config.id;
        if (key.isEmpty()) return;

        if (m_sessions.contains(key)) {
            m_sessions[key]->stop();
        }

        auto transport = m_transportFactory ? m_transportFactory->create(config) : nullptr;
        auto session = std::make_shared<McpSession>(config, std::move(transport));

        connect(session.get(), &McpSession::errorOccurred, this, [this, key](const QString& err) {
            emit serverError(key, err);
        });

        connect(session.get(), &McpSession::toolsUpdated, this, [this](const QList<domain::agent::ToolDefinition>&) {
            if (m_toolProvider) {
                m_toolProvider->refreshTools();
            }
        });

        m_sessions.insert(key, session);
    }

    void McpRuntime::onServerRegistered(const domain::mcp::McpServerConfig& config) {
        initSession(config);
    }

    void McpRuntime::onServerUnregistered(const QString& id) {
        stopServer(id);
    }

    bool McpRuntime::startServer(const QString& id) {
        if (!m_sessions.contains(id)) {
            if (!m_registry) return false;
            auto cfgOpt = m_registry->server(id);
            if (!cfgOpt.has_value()) {
                return false;
            }
            initSession(cfgOpt.value());
        }

        auto session = m_sessions.value(id);
        if (!session) return false;

        if (session->state() == domain::mcp::McpConnectionState::Ready) {
            return true;
        }

        // 安全信任检查：未批准的外部服务禁止启动
        if (!m_trustPolicy.isServerTrusted(id, session->config().autoApprove)) {
            const QString err = QStringLiteral("MCP 服务 [%1] 未获得安全信任授权，拒绝启动").arg(id);
            emit serverError(id, err);
            return false;
        }

        if (session->start()) {
            if (m_toolProvider) {
                m_toolProvider->refreshTools();
            }
            emit serverStarted(id);
            return true;
        }

        return false;
    }

    void McpRuntime::stopServer(const QString& id) {
        auto it = m_sessions.find(id);
        if (it != m_sessions.end()) {
            it.value()->stop();
            m_sessions.erase(it);
            if (m_toolProvider) {
                m_toolProvider->refreshTools();
            }
            emit serverStopped(id);
        }
    }

    bool McpRuntime::restartServer(const QString& id) {
        stopServer(id);
        return startServer(id);
    }

    void McpRuntime::startEnabledServers() {
        if (!m_registry) return;
        for (const auto& cfg : m_registry->servers()) {
            const QString key = cfg.id.isEmpty() ? cfg.name : cfg.id;
            if (cfg.isEnabled()) {
                startServer(key);
            }
        }
    }

    void McpRuntime::stopAll() {
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            it.value()->stop();
            emit serverStopped(it.key());
        }
        m_sessions.clear();
        if (m_toolProvider) {
            m_toolProvider->refreshTools();
        }
    }

    void McpRuntime::stopServersForProject(const QString& workspaceRoot) {
        if (workspaceRoot.isEmpty()) return;

        const QString canonicalRoot = QFileInfo(workspaceRoot).canonicalFilePath();
        QList<QString> toStop;

        for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
            const QString cwd = it.value()->config().cwd;
            if (!cwd.isEmpty()) {
                const QString canonicalCwd = QFileInfo(cwd).canonicalFilePath();
                if (canonicalCwd.startsWith(canonicalRoot) || canonicalRoot.startsWith(canonicalCwd)) {
                    toStop.append(it.key());
                }
            }
        }

        for (const auto& name : toStop) {
            stopServer(name);
            if (m_registry) {
                m_registry->unregisterServer(name);
            }
        }
    }

    McpSession* McpRuntime::session(const QString& id) const {
        auto it = m_sessions.find(id);
        if (it != m_sessions.end()) {
            return it.value().get();
        }
        return nullptr;
    }

    QList<McpSession*> McpRuntime::allSessions() const {
        QList<McpSession*> list;
        list.reserve(m_sessions.size());
        for (const auto& s : m_sessions) {
            list.append(s.get());
        }
        return list;
    }

    std::shared_ptr<McpToolProvider> McpRuntime::toolProvider() const {
        return m_toolProvider;
    }

    std::shared_ptr<McpResourceProvider> McpRuntime::resourceProvider() const {
        return m_resourceProvider;
    }

    std::shared_ptr<McpPromptProvider> McpRuntime::promptProvider() const {
        return m_promptProvider;
    }

} // namespace llm::mcp
