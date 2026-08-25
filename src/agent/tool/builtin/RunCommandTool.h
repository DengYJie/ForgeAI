#pragma once

#include <memory>
#include "application/ports/ITool.h"
#include "llm/workspace/WorkspaceFileSystem.h"

namespace agent::tool::builtin {

    /**
     * @brief 异步系统命令与程序执行工具
     */
    class RunCommandTool final : public application::ports::ITool {
    public:
        explicit RunCommandTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs = nullptr);
        ~RunCommandTool() override = default;

        domain::agent::ToolDefinition definition() const override;
        application::ports::ToolExecutionTraits traits() const override {
            return {true, false, false, QStringLiteral("proc:run")};
        }

        QList<domain::agent::ToolPermission> permissions(const domain::agent::ToolCall& call) const override;
        QList<domain::agent::ToolPermission> permissions() const override {
            return {{domain::agent::ToolPermissionType::ProcessExecute, QStringLiteral("执行系统命令或进程")}};
        }

        std::unique_ptr<application::ports::IToolOperation> execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

    private:
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> m_fs;
    };

} // namespace agent::tool::builtin
