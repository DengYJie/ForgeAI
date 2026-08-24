#pragma once
#include <QString>
#include <QStringList>
#include "ModelCapabilities.h"
#include "ModelParameters.h"

namespace domain::model {

    /**
     * @brief 模型 Token 上限定义
     */
    struct ModelLimit {
        int context = 128000;                 ///< 总上下文窗口大小
        int maxInput = 128000;                ///< 单次最大输入 Token 限制
        int maxOutput = 8192;                 ///< 单次最大输出 Token 限制

        bool operator==(const ModelLimit &other) const = default;
    };

    /**
     * @brief 模型模态支持
     */
    struct ModelModalities {
        QStringList input = { QStringLiteral("text") };
        QStringList output = { QStringLiteral("text") };

        bool operator==(const ModelModalities &other) const = default;
    };

    /**
     * @brief 模型本体元数据
     */
    struct CanonicalModel {
        QString id;                           ///< 模型唯一标识 (如 "deepseek/deepseek-v4-flash")
        QString name;                         ///< 展示名称
        QString family;                       ///< 架构家族
        QString description;                  ///< 模型描述
        ModelCapabilities capabilities;       ///< 能力集合
        ModelLimit limits;                    ///< 上下文与输出限制
        ModelModalities modalities;           ///< 模态支持
        ModelParameters defaultParams;        ///< 推荐默认参数
        bool openWeights = false;             ///< 是否开源权重
        QString knowledgeCutoff;              ///< 知识库截止时间
        QString releaseDate;                  ///< 发布日期

        bool operator==(const CanonicalModel &other) const = default;
    };

} // namespace domain::model
