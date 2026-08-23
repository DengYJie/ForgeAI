#pragma once
#include <optional>
#include <QString>

namespace domain::model {

    /**
     * @brief 模型推理超参数配置
     */
    struct ModelParameters {
        double temperature = 0.7;             ///< 采样温度 (0.0 ~ 2.0)
        double topP = 1.0;                    ///< Top-P 采样
        double presencePenalty = 0.0;         ///< 存在惩罚
        double frequencyPenalty = 0.0;        ///< 频率惩罚

        std::optional<int> maxOutputTokens;   ///< 单次最大输出限制 (留空则使用模型默认)

        // 深度思考/推理控制
        bool enableThinking = true;           ///< 是否开启深度思考
        QString reasoningEffort;              ///< 推理强度 ("none", "low", "medium", "high", "max")
        int thinkingBudgetTokens = 4096;      ///< 思考 Token 预算上限

        bool operator==(const ModelParameters &other) const = default;
    };

} // namespace domain::model
