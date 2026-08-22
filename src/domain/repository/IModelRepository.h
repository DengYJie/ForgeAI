#pragma once
#include <QList>
#include <optional>
#include "domain/model/ModelProvider.h"
#include "domain/model/Model.h"

namespace domain::repository {

    /**
     * @brief 模型与服务商配置仓储接口
     */
    class IModelRepository {
    public:
        virtual ~IModelRepository() = default;

        /**
         * @brief 获取所有已配置的模型服务商
         */
        virtual QList<domain::model::ModelProvider> getAllProviders() = 0;

        /**
         * @brief 根据 providerId 获取指定服务商配置
         */
        virtual std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) = 0;

        /**
         * @brief 保存或更新服务商配置
         */
        virtual void saveProvider(const domain::model::ModelProvider &provider) = 0;

        /**
         * @brief 删除服务商配置
         */
        virtual void deleteProvider(const QString &providerId) = 0;

        /**
         * @brief 获取当前所有已启用的模型列表（供 UI 快速渲染模型切换菜单）
         */
        virtual QList<domain::model::Model> getEnabledModels() = 0;

        /**
         * @brief 根据 modelId 获取模型实体详情（包含 capabilities 与 contextWindow）
         */
        virtual std::optional<domain::model::Model> getModel(const QString &modelId) = 0;
    };

} // namespace domain::repository
