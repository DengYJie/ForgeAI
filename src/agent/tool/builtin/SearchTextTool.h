#pragma once

#include <memory>
#include "application/ports/ITool.h"
#include "llm/workspace/WorkspaceFileSystem.h"

namespace agent::tool::builtin {

    class SearchTextTool final : public application::ports::ITool {
    public:
        explicit SearchTextTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs = nullptr);
        ~SearchTextTool() override = default;

        domain::agent::ToolDefinition definition() const override;
        QList<domain::agent::ToolPermission> permissions() const override {
            return {{domain::agent::ToolPermissionType::ReadOnly, QStringLiteral("搜索工作区文件文本内容")}};
        }
        domain::agent::ToolResult execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

    private:
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> m_fs;
    };

} // namespace agent::tool::builtin
