#pragma once

#include <QObject>
#include <QList>
#include <memory>
#include <mutex>
#include "domain/mcp/McpPrompt.h"
#include "McpSession.h"

namespace llm::mcp {

    class McpRuntime;

    /**
     * @brief MCP Prompt 提供者（汇总管理已激活 MCP 会话暴露的 Prompt 模板与渲染）
     */
    class McpPromptProvider : public QObject {
        Q_OBJECT
    public:
        explicit McpPromptProvider(McpRuntime* runtime = nullptr, QObject* parent = nullptr);
        ~McpPromptProvider() override = default;

        /**
         * @brief 获取所有可用 MCP Prompt 模板
         */
        QList<domain::mcp::McpPrompt> prompts() const;

        /**
         * @brief 获取并渲染指定 Prompt 的消息序列
         */
        QList<domain::mcp::McpPromptMessage> getPrompt(
            const QString& name,
            const QJsonObject& arguments = {},
            int timeoutMs = 5000
        );

    private:
        McpRuntime* m_runtime = nullptr;
        mutable std::mutex m_mutex;
    };

} // namespace llm::mcp
