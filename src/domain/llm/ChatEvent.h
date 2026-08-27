#pragma once
#include <QString>
#include <QList>
#include <variant>
#include "domain/agent/ToolExecution.h"
#include "domain/llm/ChatError.h"

namespace domain::llm {

    /**
     * @brief Token 使用量统计
     */
    struct TokenUsage {
        int inputTokens = 0;
        int outputTokens = 0;
        int totalTokens = 0;
        int cachedTokens = 0;

        bool operator==(const TokenUsage &other) const = default;
    };

    /**
     * @brief 各种流式事件类型的载荷定义
     */
    struct EventStarted {
        bool operator==(const EventStarted &other) const = default;
    };

    struct EventTextDelta {
        QString text;
        bool operator==(const EventTextDelta &other) const = default;
    };

    struct EventThinkingDelta {
        QString thought;
        bool operator==(const EventThinkingDelta &other) const = default;
    };

    struct EventToolCallStarted {
        QString id = {};
        QString functionName = {};
        QJsonObject protocolMetadata = {}; ///< 协议特定的不可丢失的延续元数据 (如 Gemini thoughtSignature)
        bool operator==(const EventToolCallStarted &other) const = default;
    };

    struct EventToolCallDelta {
        QString id;
        QString argumentsDelta;
        bool operator==(const EventToolCallDelta &other) const = default;
    };

    struct EventToolCallFinished {
        QString id;
        bool operator==(const EventToolCallFinished &other) const = default;
    };

    struct EventUsageUpdated {
        TokenUsage usage;
        bool operator==(const EventUsageUpdated &other) const = default;
    };

    struct EventFinished {
        QString finishReason; // "stop", "length", "tool_calls", etc.
        bool operator==(const EventFinished &other) const = default;
    };

    struct EventError {
        ChatError error;
        bool operator==(const EventError &other) const = default;
    };

    /**
     * @brief 统一的聊天事件变体
     */
    using ChatEvent = std::variant<
        EventStarted,
        EventTextDelta,
        EventThinkingDelta,
        EventToolCallStarted,
        EventToolCallDelta,
        EventToolCallFinished,
        EventUsageUpdated,
        EventFinished,
        EventError
    >;

} // namespace domain::llm
