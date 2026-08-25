#include "McpServerRegistry.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

namespace llm::mcp {

    McpServerRegistry::McpServerRegistry(QObject* parent) : QObject(parent) {
    }

    void McpServerRegistry::registerServer(const domain::mcp::McpServerConfig& config) {
        const QString key = config.id.isEmpty() ? config.name : config.id;
        if (key.trimmed().isEmpty()) return;

        bool isUpdate = m_servers.contains(key);
        m_servers.insert(key, config);

        core::logging::LoggingService::instance().debug(core::logging::Category::McpRegistry, QStringLiteral("注册 MCP 服务"), {
            {QStringLiteral("serverId"), key},
            {QStringLiteral("isUpdate"), isUpdate ? QStringLiteral("true") : QStringLiteral("false")}
        });

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
            core::logging::LoggingService::instance().debug(core::logging::Category::McpRegistry, QStringLiteral("批量注册 MCP 服务"), {
                {QStringLiteral("count"), QString::number(configs.size())}
            });
            emit registryChanged();
        }
    }

    void McpServerRegistry::unregisterServer(const QString& id) {
        if (m_servers.remove(id) > 0) {
            core::logging::LoggingService::instance().debug(core::logging::Category::McpRegistry, QStringLiteral("注销 MCP 服务"), {
                {QStringLiteral("serverId"), id}
            });
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
