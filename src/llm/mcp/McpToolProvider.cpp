#include "McpToolProvider.h"
#include "McpRuntime.h"

namespace llm::mcp {

    McpToolProvider::McpToolProvider(McpRuntime* runtime)
        : m_runtime(runtime) {
    }

    void McpToolProvider::addSession(McpSession* session) {
        if (!session) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_sessions.contains(session)) {
            m_sessions.append(session);
        }
    }

    void McpToolProvider::removeSession(McpSession* session) {
        if (!session) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.removeAll(session);
    }

    void McpToolProvider::clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.clear();
    }

    void McpToolProvider::refreshTools() {
        // 外部可触发刷新
    }

    QList<std::shared_ptr<application::ports::ITool>> McpToolProvider::tools() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<std::shared_ptr<application::ports::ITool>> result;

        QList<McpSession*> targetSessions = m_sessions;
        if (m_runtime) {
            for (auto* s : m_runtime->allSessions()) {
                if (s && !targetSessions.contains(s)) {
                    targetSessions.append(s);
                }
            }
        }

        for (const auto* session : targetSessions) {
            if (!session || session->state() != domain::mcp::McpConnectionState::Ready) continue;

            const auto defs = session->tools();
            const QString serverKey = session->config().id.isEmpty() ? session->config().name : session->config().id;
            for (const auto& def : defs) {
                result.append(std::make_shared<McpTool>(
                    const_cast<McpSession*>(session),
                    serverKey,
                    def
                ));
            }
        }

        return result;
    }

} // namespace llm::mcp
