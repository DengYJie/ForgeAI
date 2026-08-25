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

    std::unique_ptr<application::ports::IToolOperation> McpTool::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        if (!m_session || !m_session->client()) {
            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [this, call]() {
                    return domain::agent::ToolResult{
                        call.id,
                        QStringLiteral("MCP 服务未就绪或未连接: %1").arg(m_serverName),
                        true
                    };
                }
            );
        }

        const int timeoutMs = context.timeoutMs > 0 ? context.timeoutMs : 30000;
        return m_session->client()->callToolAsync(
            call.id,
            m_originalToolName,
            call.arguments,
            timeoutMs,
            context.cancellationToken
        );
    }

} // namespace llm::mcp
