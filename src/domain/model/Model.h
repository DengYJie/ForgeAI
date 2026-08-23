#pragma once
#include <QString>
#include <QStringList>
#include <optional>
#include "ModelCapabilities.h"
#include "ModelParameters.h"

namespace domain::model {

    /**
     * @brief 模型 Token 与物理上限定义 (对齐 models.dev limit)
     */
    struct ModelLimit {
        int context = 128000;                 ///< 总上下文窗口大小
        int maxInput = 128000;                ///< 单次最大输入 Token 限制
        int maxOutput = 8192;                 ///< 单次最大输出 Token 限制

        bool operator==(const ModelLimit &other) const = default;
    };

    /**
     * @brief 模型计费信息 (按每 100 万 Tokens 计算，包含 Prompt Caching 读写计费)
     */
    struct ModelPricing {
        double inputPrice = 0.0;              ///< 基础输入价格 / 1M tokens
        double outputPrice = 0.0;             ///< 基础输出价格 / 1M tokens
        double cacheReadPrice = 0.0;          ///< Prompt Cache 命中读取价格 / 1M tokens
        double cacheWritePrice = 0.0;         ///< Prompt Cache 写入价格 / 1M tokens
        QString currency = "USD";             ///< 货币单位 ("USD" 或 "CNY")

        bool operator==(const ModelPricing &other) const = default;
    };

    /**
     * @brief 具体大语言模型实体
     */
    struct Model {
        QString id;                           ///< 模型的真实 API 标识符
        QString providerId;                   ///< 归属的服务商 ID
        QString displayName;                  ///< 界面展示名称
        QString description;                  ///< 模型简介
        QString family;                       ///< 模型家族架构
        QString group;                        ///< UI 分类折叠组

        ModelLimit limits;                    ///< 上下文与输出限制
        ModelCapabilities capabilities;       ///< 模型能力标志集合
        ModelParameters defaultParams;        ///< 推荐的默认推理参数
        ModelPricing pricing;                 ///< 成本与缓存计费单价

        QString reasoningField;               ///< 思考流字段名 (如 "reasoning_content", 留空为原生 thinking)
        bool openWeights = false;             ///< 是否为开源权重模型
        QString knowledgeCutoff;              ///< 知识库截止日期

        bool isEnabled = true;                ///< 用户是否在列表中启用

        bool operator==(const Model &other) const = default;
    };

} // namespace domain::model
