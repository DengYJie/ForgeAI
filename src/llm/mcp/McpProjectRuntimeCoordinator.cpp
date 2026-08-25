#include "McpProjectRuntimeCoordinator.h"
#include "McpManager.h"
#include <QDir>

namespace llm::mcp {

    McpProjectRuntimeCoordinator::McpProjectRuntimeCoordinator(McpManager* mcpManager)
        : m_mcpManager(mcpManager) {
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
