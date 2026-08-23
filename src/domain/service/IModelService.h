#pragma once

#include <QObject>
#include <QList>
#include <optional>
#include <utility>
#include "domain/model/Model.h"
#include "domain/model/ModelProvider.h"

namespace domain::service {
    /**
     * @brief 模型与服务商管理服务接口
     */
    class IModelService : public QObject {
        Q_OBJECT

    public:
        using QObject::QObject;
        ~IModelService() override = default;

        /**
         * @brief 获取所有已激活的服务商配置列表
         */
        virtual QList<domain::model::ModelProvider> getActiveProviders() const = 0;

        /**
         * @brief 获取当前所有已启用的模型列表（供 UI 切换栏渲染）
         */
        virtual QList<domain::model::Model> getEnabledModels() const = 0;

        /**
         * @brief 根据 providerId 获取指定服务商配置
         */
        virtual std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) const = 0;

        /**
         * @brief 根据 modelId 高速解析出模型实体与其归属服务商配置
         */
        virtual std::optional<std::pair<domain::model::Model, domain::model::ModelProvider>> resolveModel(const QString &modelId) const = 0;

        /**
         * @brief 保存或更新服务商配置
         */
        virtual void saveProvider(const domain::model::ModelProvider &provider) = 0;

        /**
         * @brief 删除服务商配置
         */
        virtual void deleteProvider(const QString &providerId) = 0;

    Q_SIGNALS:
        /**
         * @brief 服务商或模型配置发生变更通知
         */
        void providersChanged();
    };
} // namespace domain::service
