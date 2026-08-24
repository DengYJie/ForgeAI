#pragma once
#include <QString>
#include <optional>
#include "CanonicalModel.h"
#include "ModelCapabilities.h"

namespace domain::model {

    /**
     * @brief 模型计费信息 (按每 100 万 Tokens 计算)
     */
    struct ModelPricing {
        double inputPrice = 0.0;              ///< 基础输入价格 / 1M tokens
        double outputPrice = 0.0;             ///< 基础输出价格 / 1M tokens
        double cacheReadPrice = 0.0;          ///< Prompt Cache 命中读取价格 / 1M tokens
        double cacheWritePrice = 0.0;         ///< Prompt Cache 写入价格 / 1M tokens
        QString currency = QStringLiteral("USD"); ///< 货币单位 ("USD" 或 "CNY")

        bool operator==(const ModelPricing &other) const = default;
    };

    /**
     * @brief 数据来源标识
     */
    enum class DataOrigin {
        BuiltIn,    ///< 系统内置预设
        User        ///< 用户自定义
    };

    /**
     * @brief 服务商模型挂载实体
     */
    struct ProviderModel {
        QString providerId;                   ///< 归属服务商 ID
        QString remoteModelId;                ///< 实际请求 API 时发送的模型标识
        std::optional<QString> canonicalModelId; ///< 关联的模型本体 ID

        ModelPricing pricing;                 ///< 计费单价
        std::optional<ModelLimit> limitsOverride; ///< 上下文与输出上限覆盖
        std::optional<ModelCapabilities> capabilitiesOverride; ///< 能力开关覆盖

        QString reasoningField;               ///< 思考流专用字段名
        QString group;                        ///< 分组名称
        bool isEnabled = true;                ///< 是否启用
        bool isCustom = false;                ///< 是否为用户自定义添加
        DataOrigin origin = DataOrigin::BuiltIn;

        bool operator==(const ProviderModel &other) const = default;
    };

} // namespace domain::model
