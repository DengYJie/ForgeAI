#include "SwitchProjectUseCase.h"
#include "domain/service/IProjectContextService.h"

namespace application::usecase::work {

    SwitchProjectUseCase::SwitchProjectUseCase(
        application::ports::IProjectRuntimeCoordinator* runtimeCoordinator,
        domain::service::IProjectContextService* projectContextService
    ) : m_runtimeCoordinator(runtimeCoordinator),
        m_projectContextService(projectContextService) {
    }

    void SwitchProjectUseCase::stopProjectRuntime(const QString& projectRoot) const {
        if (!projectRoot.isEmpty() && m_runtimeCoordinator) {
            m_runtimeCoordinator->unloadProject(projectRoot);
        }
    }

    domain::project::ProjectContext SwitchProjectUseCase::execute(
        const QString& previousProjectRoot,
        const QString& newProjectRoot
    ) const {
        if (m_runtimeCoordinator) {
            m_runtimeCoordinator->switchProject(previousProjectRoot, newProjectRoot);
        }

        if (m_projectContextService && !newProjectRoot.isEmpty()) {
            return m_projectContextService->load(newProjectRoot);
        }

        return {};
    }

} // namespace application::usecase::work
