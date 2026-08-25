#include "McpToolProvider.h"

namespace llm::mcp {

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

    QList<std::shared_ptr<application::ports::ITool>> McpToolProvider::tools() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<std::shared_ptr<application::ports::ITool>> result;

        for (const auto* session : m_sessions) {
            if (!session || session->state() != McpSessionState::Connected) continue;

            const auto defs = session->tools();
            for (const auto& def : defs) {
                result.append(std::make_shared<McpTool>(const_cast<McpSession*>(session), def));
            }
        }

        return result;
    }

} // namespace llm::mcp
