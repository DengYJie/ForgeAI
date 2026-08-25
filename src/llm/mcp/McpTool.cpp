#include "McpTool.h"

namespace llm::mcp {

    McpTool::McpTool(
        McpSession* session,
        const QString& serverName,
        const domain::agent::ToolDefinition& originalDef
    ) : m_session(session),
        m_serverName(serverName),
        m_originalToolName(originalDef.name) {

        m_namespacedDef = originalDef;
        m_namespacedDef.name = QStringLiteral("mcp::%1::%2").arg(serverName, originalDef.name);
    }

    domain::agent::ToolDefinition McpTool::definition() const {
        return m_namespacedDef;
    }

    QString McpTool::serverName() const {
        return m_serverName;
    }

    QString McpTool::originalToolName() const {
        return m_originalToolName;
    }

    domain::agent::ToolResult McpTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        Q_UNUSED(context);
        if (!m_session || !m_session->client()) {
            return domain::agent::ToolResult{
                call.id,
                QStringLiteral("MCP 服务未就绪或未连接: %1").arg(m_serverName),
                true
            };
        }

        // 统一转发给底层客户端时使用 MCP 服务的原始工具名
        return m_session->client()->callTool(call.id, m_originalToolName, call.arguments);
    }

} // namespace llm::mcp
