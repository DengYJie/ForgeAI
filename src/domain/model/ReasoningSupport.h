#pragma once

#include <optional>
#include <QStringList>

namespace domain::model {

    /**
     * @brief 模型推理能力与思考预算配置支持结构
     */
    struct ReasoningSupport {
        bool supported = false;

        QStringList effortLevels; ///< 支持的推理强度选项 (如 ["low", "medium", "high"])

        std::optional<int> minBudgetTokens;     ///< 思考 Token 预算下限
        std::optional<int> maxBudgetTokens;     ///< 思考 Token 预算上限
        std::optional<int> defaultBudgetTokens; ///< 默认思考 Token 预算

        bool operator==(const ReasoningSupport &) const = default;
    };

} // namespace domain::model
