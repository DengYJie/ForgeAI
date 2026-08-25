#pragma once

#include "application/ports/IProjectRuntimeCoordinator.h"

namespace llm::mcp {

    class McpManager;

    /**
     * @brief MCP 项目运行时协调器
     * @details 专职在项目切换/卸载时清理 MCP 外部服务进程。
     */
    class McpProjectRuntimeCoordinator final : public application::ports::IProjectRuntimeCoordinator {
    public:
        explicit McpProjectRuntimeCoordinator(McpManager* mcpManager = nullptr);
        ~McpProjectRuntimeCoordinator() override = default;

        void switchProject(
            const QString& previousProjectRoot,
            const QString& newProjectRoot
        ) override;

        void unloadProject(
            const QString& projectRoot
        ) override;

    private:
        McpManager* m_mcpManager = nullptr;
    };

} // namespace llm::mcp
