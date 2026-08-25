#include "SwitchProjectUseCase.h"
#include "llm/mcp/McpManager.h"
#include "domain/service/IProjectContextService.h"
#include <QDir>

namespace application::usecase::work {

    SwitchProjectUseCase::SwitchProjectUseCase(
        llm::mcp::McpManager* mcpManager,
        domain::service::IProjectContextService* projectContextService
    ) : m_mcpManager(mcpManager),
        m_projectContextService(projectContextService) {
    }

    void SwitchProjectUseCase::stopProjectRuntime(const QString& projectRoot) const {
        if (!projectRoot.isEmpty() && m_mcpManager) {
            m_mcpManager->stopServersForProject(projectRoot);
        }
    }

    domain::project::ProjectContext SwitchProjectUseCase::execute(
        const QString& previousProjectRoot,
        const QString& newProjectRoot
    ) const {
        const QString canonicalNew = QDir(newProjectRoot).canonicalPath();
        const QString canonicalPrev = QDir(previousProjectRoot).canonicalPath();

        if (!canonicalPrev.isEmpty() && canonicalPrev != canonicalNew) {
            stopProjectRuntime(previousProjectRoot);
        }

        if (m_projectContextService && !newProjectRoot.isEmpty()) {
            return m_projectContextService->load(newProjectRoot);
        }

        return {};
    }

} // namespace application::usecase::work
