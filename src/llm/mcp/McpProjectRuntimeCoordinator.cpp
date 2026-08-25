#include "McpProjectRuntimeCoordinator.h"
#include "McpManager.h"
#include <QDir>

namespace llm::mcp {

    McpProjectRuntimeCoordinator::McpProjectRuntimeCoordinator(McpManager* mcpManager)
        : m_mcpManager(mcpManager) {
    }

    void McpProjectRuntimeCoordinator::loadProject(
        const QString& projectRoot,
        const QStringList& enabledServers
    ) {
        if (!m_mcpManager || projectRoot.isEmpty()) return;

        const QStringList candidateConfigFiles = {
            QDir(projectRoot).filePath(QStringLiteral(".mcp.json")),
            QDir(projectRoot).filePath(QStringLiteral("mcp.json"))
        };

        for (const auto& cfgPath : candidateConfigFiles) {
            if (QFile::exists(cfgPath)) {
                const auto serverConfigs = m_mcpManager->parseConfigFile(cfgPath);
                for (auto sCfg : serverConfigs) {
                    const QString serverId = sCfg.id.isEmpty() ? sCfg.name : sCfg.id;
                    if (!enabledServers.isEmpty() && !enabledServers.contains(serverId) && !enabledServers.contains(sCfg.name)) {
                        continue;
                    }
                    if (sCfg.cwd.isEmpty()) sCfg.cwd = projectRoot;
                    m_mcpManager->registerServer(sCfg);
                    if (m_mcpManager->trustPolicy().isServerTrusted(serverId, sCfg.autoApprove)) {
                        m_mcpManager->startServer(serverId);
                    }
                }
            }
        }
    }

    void McpProjectRuntimeCoordinator::switchProject(
        const QString& previousProjectRoot,
        const QString& newProjectRoot
    ) {
        const QString canonicalNew = QDir(newProjectRoot).canonicalPath();
        const QString canonicalPrev = QDir(previousProjectRoot).canonicalPath();

        if (!canonicalPrev.isEmpty() && canonicalPrev != canonicalNew) {
            unloadProject(previousProjectRoot);
        }
    }

    void McpProjectRuntimeCoordinator::unloadProject(const QString& projectRoot) {
        if (!projectRoot.isEmpty() && m_mcpManager) {
            m_mcpManager->stopServersForProject(projectRoot);
        }
    }

} // namespace llm::mcp
