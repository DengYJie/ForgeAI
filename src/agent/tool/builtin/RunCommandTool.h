#pragma once

#include "application/ports/ITool.h"
#include "application/ports/IProcessTaskRuntime.h"
#include "application/ports/IShellService.h"
#include "llm/workspace/WorkspaceFileSystem.h"
#include <memory>

namespace agent::tool::builtin {

    /**
     * @brief 执行系统 Shell 命令行工具（支持前台等待与后台常驻双模式）
     */
    class RunCommandTool : public application::ports::ITool {
    public:
        explicit RunCommandTool(
            std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs
        );

        explicit RunCommandTool(
            std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime,
            std::shared_ptr<application::ports::IShellService> shellService = nullptr,
            std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs = nullptr
        );

        domain::agent::ToolDefinition definition() const override;
        application::ports::ToolExecutionTraits traits() const override;
        QList<domain::agent::ToolPermission> permissions(const domain::agent::ToolCall& call) const override;

        std::unique_ptr<application::ports::IToolOperation> execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

    private:
        std::shared_ptr<application::ports::IProcessTaskRuntime> m_taskRuntime;
        std::shared_ptr<application::ports::IShellService> m_shellService;
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> m_fs;
    };

} // namespace agent::tool::builtin
