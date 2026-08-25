#pragma once

#include <memory>
#include "application/ports/ITool.h"
#include "llm/workspace/WorkspaceFileSystem.h"

namespace agent::tool::builtin {

    class ListFilesTool final : public application::ports::ITool {
    public:
        explicit ListFilesTool(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs = nullptr);
        ~ListFilesTool() override = default;

        domain::agent::ToolDefinition definition() const override;
        QList<domain::agent::ToolPermission> permissions() const override {
            return {{domain::agent::ToolPermissionType::ReadOnly, QStringLiteral("遍历工作区文件目录")}};
        }
        domain::agent::ToolResult execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

    private:
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> m_fs;
    };

} // namespace agent::tool::builtin
