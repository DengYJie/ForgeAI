#pragma once

#include <memory>
#include "application/ports/ITool.h"
#include "llm/workspace/WorkspaceFileSystem.h"

namespace agent::tool::builtin {

    /**
     * @brief 结构化代码补丁工具（通过精确匹配 old_text 替换为 new_text 进行精准修改）
     */
    class ApplyPatchTool final : public application::ports::ITool {
    public:
        explicit ApplyPatchTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs = nullptr);
        ~ApplyPatchTool() override = default;

        domain::agent::ToolDefinition definition() const override;
        application::ports::ToolExecutionTraits traits() const override {
            return {true, false, false, QStringLiteral("fs:patch")};
        }
        QList<domain::agent::ToolPermission> permissions() const override {
            return {{domain::agent::ToolPermissionType::FileSystemWrite, QStringLiteral("修改工作区代码文件")}};
        }
        std::unique_ptr<application::ports::IToolOperation> execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

    private:
        domain::agent::ToolResult executeInternal(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        );

        std::shared_ptr<llm::workspace::WorkspaceFileSystem> m_fs;
    };

} // namespace agent::tool::builtin
