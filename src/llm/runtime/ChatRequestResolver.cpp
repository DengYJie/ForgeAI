#include "ChatRequestResolver.h"
#include <algorithm>

namespace llm::runtime {

    domain::llm::ResolvedChatOptions ChatRequestResolver::resolve(
        const domain::model::ResolvedModel &model,
        const domain::llm::ChatRequest &request
    ) {
        domain::llm::ResolvedChatOptions options;
        const auto caps = model.effectiveCapabilities();
        const auto limits = model.effectiveLimits();

        // 1. Temperature 解析
        if (request.temperature.has_value()) {
            options.temperature = request.temperature;
        } else if (model.canonical.has_value()) {
            options.temperature = model.canonical->defaultParams.temperature;
        }

        // 2. Top-P 解析
        if (model.canonical.has_value()) {
            options.topP = model.canonical->defaultParams.topP;
        }

        // 3. Max Output Tokens 上限夹紧与解析
        if (request.maxTokens.has_value()) {
            options.maxOutputTokens = std::min(*request.maxTokens, limits.maxOutput);
        } else if (model.canonical && model.canonical->defaultParams.maxOutputTokens.has_value()) {
            options.maxOutputTokens = std::min(*model.canonical->defaultParams.maxOutputTokens, limits.maxOutput);
        }

        // 4. 深度思考 (Thinking / Reasoning) 解析
        const bool supportsThinking = caps.testFlag(domain::model::ModelCapability::Thinking);
        options.thinkingEnabled = request.useDeepThinking && supportsThinking;

        if (options.thinkingEnabled) {
            if (!request.reasoningEffort.isEmpty()) {
                options.reasoningEffort = request.reasoningEffort;
            } else if (model.canonical) {
                options.reasoningEffort = model.canonical->defaultParams.reasoningEffort;
            }

            if (model.canonical) {
                options.thinkingBudgetTokens = model.canonical->defaultParams.thinkingBudgetTokens;
            } else {
                options.thinkingBudgetTokens = 4096;
            }
        }

        // 5. 工具调用 (Tool Calling) 门控
        options.toolsEnabled = request.tools.has_value() &&
                               !request.tools->isEmpty() &&
                               caps.testFlag(domain::model::ModelCapability::ToolCalling);

        // 6. 联网搜索 (Web Search)
        options.webSearchEnabled = request.useWebSearch;

        return options;
    }

} // namespace llm::runtime
