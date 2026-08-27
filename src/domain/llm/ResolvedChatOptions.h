#pragma once

#include <optional>
#include <QString>

namespace domain::llm {

    /**
     * @brief 基于模型能力和请求意图解析后的最终语义参数
     * @details 剥离所有特定协议的字段名（如 thinkingConfig、budget_tokens、reasoning_effort 等），
     *          由 IProtocolAdapter 负责将其转换为具体的协议载荷。
     */
    struct ResolvedChatOptions {
        std::optional<double> temperature;
        std::optional<double> topP;

        std::optional<int> maxOutputTokens;

        bool thinkingEnabled = false;
        QString reasoningEffort;
        std::optional<int> thinkingBudgetTokens;

        bool toolsEnabled = false;
        bool webSearchEnabled = false;

        bool operator==(const ResolvedChatOptions &) const = default;
    };

} // namespace domain::llm
