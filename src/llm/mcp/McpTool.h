#pragma once

#include "application/ports/ITool.h"
#include "McpSession.h"
#include <memory>

namespace llm::mcp {

    /**
     * @brief MCP 工具适配器（将 MCP 暴露的工具包装为 ForgeAI 统一的 ITool 接口，并支持命名空间隔离）
     */
    class McpTool final : public application::ports::ITool {
    public:
        McpTool(
            McpSession* session,
            const QString& serverName,
            const domain::agent::ToolDefinition& originalDef
        );
        ~McpTool() override = default;

        domain::agent::ToolDefinition definition() const override;
        QList<domain::agent::ToolPermission> permissions() const override {
            return {{domain::agent::ToolPermissionType::ExternalService, QStringLiteral("调用外部 MCP 服务: %1").arg(m_serverName)}};
        }
        bool isThreadSafe() const override {
            return false;
        }
        domain::agent::ToolResult execute(
            const domain::agent::ToolCall& call,
            const application::ports::ToolExecutionContext& context
        ) override;

        QString serverName() const;
        QString originalToolName() const;

    private:
        McpSession* m_session = nullptr;
        QString m_serverName;
        QString m_originalToolName;
        domain::agent::ToolDefinition m_namespacedDef;
    };

} // namespace llm::mcp
