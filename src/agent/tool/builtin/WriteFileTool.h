#pragma once

#include <memory>
#include "application/ports/ITool.h"
#include "llm/workspace/WorkspaceFileSystem.h"

namespace agent::tool::builtin {

    class WriteFileTool final : public application::ports::ITool {
    public:
        explicit WriteFileTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs = nullptr);
        ~WriteFileTool() override = default;

        domain::agent::ToolDefinition definition() const override;
        QList<domain::agent::ToolPermission> permissions() const override {
            return {{domain::agent::ToolPermissionType::WriteWorkspace, QStringLiteral("写入或创建工作区文件")}};
        }
        domain::agent::ToolResult execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

    private:
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> m_fs;
    };

} // namespace agent::tool::builtin
