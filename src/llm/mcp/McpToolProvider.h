#pragma once

#include "application/ports/IToolProvider.h"
#include "McpSession.h"
#include "McpTool.h"
#include <QList>
#include <memory>
#include <mutex>

namespace llm::mcp {

    class McpRuntime;

    /**
     * @brief MCP 工具提供者（汇总管理已激活 MCP 会话的工具列表并暴露给 ToolRegistry）
     */
    class McpToolProvider final : public application::ports::IToolProvider {
    public:
        explicit McpToolProvider(McpRuntime* runtime = nullptr);
        ~McpToolProvider() override = default;

        void addSession(McpSession* session);
        void removeSession(McpSession* session);
        void clear();
        void refreshTools();

        QList<std::shared_ptr<application::ports::ITool>> tools() const override;

    private:
        McpRuntime* m_runtime = nullptr;
        mutable std::mutex m_mutex;
        QList<McpSession*> m_sessions;
    };

} // namespace llm::mcp
