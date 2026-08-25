#pragma once

#include <QString>
#include "domain/project/ProjectContext.h"
#include "application/ports/IProjectRuntimeCoordinator.h"

namespace domain::service {
    class IProjectContextService;
}

namespace application::usecase::work {

    /**
     * @brief 切换工作区项目用例
     * @details 通过 IProjectRuntimeCoordinator 协调项目运行时的释放与切换，并加载新项目的上下文定义。
     */
    class SwitchProjectUseCase {
    public:
        explicit SwitchProjectUseCase(
            application::ports::IProjectRuntimeCoordinator* runtimeCoordinator = nullptr,
            domain::service::IProjectContextService* projectContextService = nullptr
        );

        domain::project::ProjectContext execute(
            const QString& previousProjectRoot,
            const QString& newProjectRoot
        ) const;

        void stopProjectRuntime(const QString& projectRoot) const;

    private:
        application::ports::IProjectRuntimeCoordinator* m_runtimeCoordinator = nullptr;
        domain::service::IProjectContextService* m_projectContextService = nullptr;
    };

} // namespace application::usecase::work
