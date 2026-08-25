#include "McpManager.h"

namespace llm::mcp {

    McpManager::McpManager(QObject* parent)
        : QObject(parent),
          m_registry(std::make_unique<McpServerRegistry>(this)),
          m_runtime(std::make_unique<McpRuntime>(m_registry.get(), nullptr, this)) {

        connect(m_runtime.get(), &McpRuntime::serverStarted, this, &McpManager::serverStarted);
        connect(m_runtime.get(), &McpRuntime::serverStopped, this, &McpManager::serverStopped);
        connect(m_runtime.get(), &McpRuntime::serverError, this, &McpManager::serverError);
    }

    McpServerRegistry* McpManager::registry() const {
        return m_registry.get();
    }

    McpRuntime* McpManager::runtime() const {
        return m_runtime.get();
    }

    QList<domain::mcp::McpServerConfig> McpManager::parseConfigFile(const QString& filePath) {
        return McpConfigLoader::loadFromFile(filePath).configs;
    }

    QList<domain::mcp::McpServerConfig> McpManager::parseConfigContent(const QString& jsonContent) {
        return McpConfigLoader::loadFromJsonString(jsonContent).configs;
    }

    void McpManager::registerServer(const domain::mcp::McpServerConfig& config) {
        if (m_registry) {
            m_registry->registerServer(config);
        }
    }

    void McpManager::unregisterServer(const QString& name) {
        if (m_runtime) {
            m_runtime->stopServer(name);
        }
        if (m_registry) {
            m_registry->unregisterServer(name);
        }
    }

    bool McpManager::startServer(const QString& name) {
        if (m_runtime) {
            return m_runtime->startServer(name);
        }
        return false;
    }

    void McpManager::stopServer(const QString& name) {
        if (m_runtime) {
            m_runtime->stopServer(name);
        }
    }

    void McpManager::startAll() {
        if (m_runtime) {
            m_runtime->startEnabledServers();
        }
    }

    void McpManager::stopAll() {
        if (m_runtime) {
            m_runtime->stopAll();
        }
    }

    void McpManager::stopServersForProject(const QString& workspaceRoot) {
        if (m_runtime) {
            m_runtime->stopServersForProject(workspaceRoot);
        }
    }

    std::shared_ptr<McpToolProvider> McpManager::toolProvider() const {
        if (m_runtime) {
            return m_runtime->toolProvider();
        }
        return nullptr;
    }

    McpSession* McpManager::getSession(const QString& name) const {
        if (m_runtime) {
            return m_runtime->session(name);
        }
        return nullptr;
    }

    QList<McpSession*> McpManager::allSessions() const {
        if (m_runtime) {
            return m_runtime->allSessions();
        }
        return {};
    }

} // namespace llm::mcp
