#include "McpServerRegistry.h"

namespace llm::mcp {

    McpServerRegistry::McpServerRegistry(QObject* parent) : QObject(parent) {
    }

    void McpServerRegistry::registerServer(const domain::mcp::McpServerConfig& config) {
        const QString key = config.id.isEmpty() ? config.name : config.id;
        if (key.trimmed().isEmpty()) return;

        bool isUpdate = m_servers.contains(key);
        m_servers.insert(key, config);

        emit serverRegistered(config);
        emit registryChanged();
    }

    void McpServerRegistry::registerServers(const QList<domain::mcp::McpServerConfig>& configs) {
        bool changed = false;
        for (const auto& cfg : configs) {
            const QString key = cfg.id.isEmpty() ? cfg.name : cfg.id;
            if (key.trimmed().isEmpty()) continue;
            m_servers.insert(key, cfg);
            emit serverRegistered(cfg);
            changed = true;
        }
        if (changed) {
            emit registryChanged();
        }
    }

    void McpServerRegistry::unregisterServer(const QString& id) {
        if (m_servers.remove(id) > 0) {
            emit serverUnregistered(id);
            emit registryChanged();
        }
    }

    std::optional<domain::mcp::McpServerConfig> McpServerRegistry::server(const QString& id) const {
        auto it = m_servers.find(id);
        if (it != m_servers.end()) {
            return it.value();
        }
        return std::nullopt;
    }

    bool McpServerRegistry::hasServer(const QString& id) const {
        return m_servers.contains(id);
    }

    QList<domain::mcp::McpServerConfig> McpServerRegistry::servers() const {
        return m_servers.values();
    }

    void McpServerRegistry::clear() {
        if (!m_servers.isEmpty()) {
            m_servers.clear();
            emit registryChanged();
        }
    }

} // namespace llm::mcp
