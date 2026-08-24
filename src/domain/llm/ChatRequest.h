#pragma once
#include <QString>
#include <QList>
#include <optional>
#include "domain/llm/ChatMessage.h"
#include "domain/agent/ToolDefinition.h"

namespace domain::llm {

    /**
     * @brief 面向 LLM 网关的统一请求对象
     */
    struct ChatRequest {
        QString model; ///< 模型名称标识，例如 "gpt-4o"
        QList<ChatMessage> messages; ///< 对话上下文

        std::optional<bool> stream; ///< 是否开启流式响应 (通常固定为 true)
        std::optional<double> temperature; ///< 温度参数
        std::optional<int> maxTokens; ///< 生成最大 Token 数
        std::optional<QList<QString>> stop; ///< 停止词序列

        // 未来为 Function Calling 预留
        std::optional<QList<domain::agent::ToolDefinition>> tools;
        bool useWebSearch = false;
        bool useDeepThinking = false;
        QString reasoningEffort;

        bool operator==(const ChatRequest &other) const = default;
    };

} // namespace domain::llm
