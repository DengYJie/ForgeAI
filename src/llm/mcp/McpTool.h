#pragma once

#include "application/ports/ITool.h"
#include "McpSession.h"
#include <memory>

namespace llm::mcp {

    /**
     * @brief MCP 工具适配器（将 MCP 暴露的工具包装为 ForgeAI 统一的 ITool 接口）
     */
    class McpTool final : public application::ports::ITool {
    public:
        McpTool(McpSession* session, const domain::agent::ToolDefinition& def);
        ~McpTool() override = default;

        domain::agent::ToolDefinition definition() const override;
        QList<domain::agent::ToolPermission> permissions() const override {
            return {{domain::agent::ToolPermissionType::ExternalService, QStringLiteral("调用外部 MCP 工具服务")}};
        }
        domain::agent::ToolResult execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

    private:
        McpSession* m_session = nullptr;
        domain::agent::ToolDefinition m_def;
    };

} // namespace llm::mcp
