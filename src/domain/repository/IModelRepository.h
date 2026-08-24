#pragma once
#include <QList>
#include <optional>
#include "domain/model/ModelProvider.h"
#include "domain/model/CanonicalModel.h"
#include "domain/model/ProviderModel.h"
#include "domain/model/ResolvedModel.h"
#include "domain/model/Model.h"

namespace domain::repository {

    /**
     * @brief 模型与服务商配置仓储接口
     */
    class IModelRepository {
    public:
        virtual ~IModelRepository() = default;

        // --- Providers ---
        /**
         * @brief 获取所有已配置的模型服务商
         */
        virtual QList<domain::model::ModelProvider> getAllProviders() = 0;

        /**
         * @brief 根据 providerId 获取指定服务商配置
         */
        virtual std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) = 0;

        /**
         * @brief 保存或更新服务商配置（若已存在则更新属性，保留自定义 API Key）
         */
        virtual void saveProvider(const domain::model::ModelProvider &provider) = 0;

        /**
         * @brief 删除服务商配置（级联删除其下的 ProviderModel）
         */
        virtual void deleteProvider(const QString &providerId) = 0;

        // --- Canonical Models ---
        /**
         * @brief 根据 canonicalId 获取模型本体元数据
         */
        virtual std::optional<domain::model::CanonicalModel> getCanonicalModel(const QString &modelId) = 0;

        /**
         * @brief 获取所有模型本体元数据
         */
        virtual QList<domain::model::CanonicalModel> getAllCanonicalModels() = 0;

        // --- Provider Models (挂载与定制) ---
        /**
         * @brief 获取指定服务商下的所有挂载模型配置
         */
        virtual QList<domain::model::ProviderModel> getProviderModels(const QString &providerId) = 0;

        /**
         * @brief 保存或更新挂载模型配置
         */
        virtual void saveProviderModel(const domain::model::ProviderModel &binding) = 0;

        /**
         * @brief 删除指定服务商下的挂载模型
         */
        virtual void deleteProviderModel(const QString &providerId, const QString &remoteModelId) = 0;

        // --- Resolved Models (运行时组合投影) ---
        /**
         * @brief 获取指定服务商下解析后的完整模型视图列表
         */
        virtual QList<domain::model::ResolvedModel> getResolvedModelsForProvider(const QString &providerId) = 0;

        /**
         * @brief 获取所有解析后的完整模型视图列表
         */
        virtual QList<domain::model::ResolvedModel> getAllResolvedModels() = 0;

        /**
         * @brief 获取当前所有已启用的完整模型列表（服务商启用 且 挂载模型启用）
         */
        virtual QList<domain::model::ResolvedModel> getEnabledResolvedModels() = 0;

        /**
         * @brief 根据 providerId 与 remoteModelId 解析单个模型的运行时视图
         */
        virtual std::optional<domain::model::ResolvedModel> resolveModel(const QString &providerId, const QString &remoteModelId) = 0;

        // --- 兼容层 ---
        /**
         * @brief 兼容旧接口：获取当前所有已启用的模型列表
         */
        virtual QList<domain::model::Model> getEnabledModels() = 0;

        /**
         * @brief 兼容旧接口：根据 modelId 获取模型实体详情
         */
        virtual std::optional<domain::model::Model> getModel(const QString &modelId) = 0;
    };

} // namespace domain::repository
