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
        application::ports::ToolExecutionTraits traits() const override {
            return {true, false, false, QStringLiteral("fs:write")};
        }
        QList<domain::agent::ToolPermission> permissions() const override {
            return {{domain::agent::ToolPermissionType::FileSystemWrite, QStringLiteral("写入或创建工作区文件")}};
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
