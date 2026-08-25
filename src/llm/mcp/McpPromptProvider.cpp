#include "McpPromptProvider.h"
#include "McpRuntime.h"

namespace llm::mcp {

    McpPromptProvider::McpPromptProvider(McpRuntime* runtime, QObject* parent)
        : QObject(parent), m_runtime(runtime) {
    }

    QList<domain::mcp::McpPrompt> McpPromptProvider::prompts() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<domain::mcp::McpPrompt> list;
        if (!m_runtime) return list;

        for (auto* session : m_runtime->allSessions()) {
            if (session && session->state() == domain::mcp::McpConnectionState::Ready && session->client()) {
                list.append(session->client()->listPrompts());
            }
        }
        return list;
    }

    QList<domain::mcp::McpPromptMessage> McpPromptProvider::getPrompt(
        const QString& name,
        const QJsonObject& arguments,
        int timeoutMs
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_runtime) return {};

        for (auto* session : m_runtime->allSessions()) {
            if (session && session->state() == domain::mcp::McpConnectionState::Ready && session->client()) {
                auto messages = session->client()->getPrompt(name, arguments, timeoutMs);
                if (!messages.isEmpty()) {
                    return messages;
                }
            }
        }
        return {};
    }

} // namespace llm::mcp
