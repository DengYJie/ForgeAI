#include "McpTool.h"

namespace llm::mcp {

    McpTool::McpTool(McpSession* session, const domain::agent::ToolDefinition& def)
        : m_session(session), m_def(def) {
    }

    domain::agent::ToolDefinition McpTool::definition() const {
        return m_def;
    }

    domain::agent::ToolResult McpTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        Q_UNUSED(context);
        if (!m_session || !m_session->client()) {
            return domain::agent::ToolResult{
                call.id,
                QStringLiteral("MCP 会话未就绪"),
                true
            };
        }

        return m_session->client()->callTool(call.id, call.name, call.arguments);
    }

} // namespace llm::mcp
