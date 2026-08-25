#include "McpResourceProvider.h"
#include "McpRuntime.h"

namespace llm::mcp {

    McpResourceProvider::McpResourceProvider(McpRuntime* runtime, QObject* parent)
        : QObject(parent), m_runtime(runtime) {
    }

    QList<domain::mcp::McpResource> McpResourceProvider::resources() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<domain::mcp::McpResource> list;
        if (!m_runtime) return list;

        for (auto* session : m_runtime->allSessions()) {
            if (session && session->state() == domain::mcp::McpConnectionState::Ready && session->client()) {
                list.append(session->client()->listResources());
            }
        }
        return list;
    }

    std::optional<domain::mcp::McpResourceContent> McpResourceProvider::readResource(const QString& uri, int timeoutMs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_runtime) return std::nullopt;

        for (auto* session : m_runtime->allSessions()) {
            if (session && session->state() == domain::mcp::McpConnectionState::Ready && session->client()) {
                auto content = session->client()->readResource(uri, timeoutMs);
                if (content.has_value()) {
                    return content;
                }
            }
        }
        return std::nullopt;
    }

} // namespace llm::mcp
