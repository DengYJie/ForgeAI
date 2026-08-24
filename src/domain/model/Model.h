#pragma once
#include <QString>
#include <QStringList>
#include <optional>
#include "ModelCapabilities.h"
#include "ModelParameters.h"
#include "CanonicalModel.h"
#include "ProviderModel.h"
#include "ResolvedModel.h"

namespace domain::model {

    /**
     * @brief 兼容层：大语言模型综合实体 (可由 ResolvedModel 映射)
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
        bool isCustom = false;                ///< 是否为用户自定义添加的模型
        DataOrigin origin = DataOrigin::BuiltIn;

        bool operator==(const Model &other) const = default;

        static Model fromResolved(const ResolvedModel &resolved) {
            Model m;
            m.id = resolved.requestModelId();
            m.providerId = resolved.provider.id;
            m.displayName = resolved.displayName();
            m.description = resolved.description();
            m.family = resolved.family();
            m.group = resolved.group();
            m.limits = resolved.effectiveLimits();
            m.capabilities = resolved.effectiveCapabilities();
            if (resolved.canonical.has_value()) {
                m.defaultParams = resolved.canonical->defaultParams;
                m.openWeights = resolved.canonical->openWeights;
                m.knowledgeCutoff = resolved.canonical->knowledgeCutoff;
            }
            m.pricing = resolved.pricing();
            m.reasoningField = resolved.reasoningField();
            m.isEnabled = resolved.isEnabled();
            m.isCustom = resolved.isCustom();
            m.origin = resolved.origin();
            return m;
        }
    };

} // namespace domain::model
