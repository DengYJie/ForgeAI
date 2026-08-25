#pragma once

#include <QString>
#include "domain/project/ProjectContext.h"

namespace domain::service {
    class IProjectContextService;
}

namespace llm::mcp {
    class McpManager;
}

namespace application::usecase::work {

    /**
     * @brief 切换工作区项目用例
     * @details 负责协调项目切换时的基础设施与运行时释放（如安全卸载旧项目的 MCP 进程服务），并加载新项目的上下文定义。
     */
    class SwitchProjectUseCase {
    public:
        explicit SwitchProjectUseCase(
            llm::mcp::McpManager* mcpManager = nullptr,
            domain::service::IProjectContextService* projectContextService = nullptr
        );

        domain::project::ProjectContext execute(
            const QString& previousProjectRoot,
            const QString& newProjectRoot
        ) const;

        void stopProjectRuntime(const QString& projectRoot) const;

    private:
        llm::mcp::McpManager* m_mcpManager = nullptr;
        domain::service::IProjectContextService* m_projectContextService = nullptr;
    };

} // namespace application::usecase::work
