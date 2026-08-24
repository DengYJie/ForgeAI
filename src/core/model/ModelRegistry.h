#pragma once
#include <QObject>
#include <QHash>
#include <QList>
#include <optional>
#include <memory>
#include <utility>

#include "domain/model/CanonicalModel.h"
#include "domain/model/ModelProvider.h"
#include "domain/model/ProviderModel.h"
#include "domain/model/ResolvedModel.h"
#include "domain/model/Model.h"
#include "domain/repository/IModelRepository.h"

namespace core::model {

    /**
     * @brief 模型与服务商全局注册管理中心
     */
    class ModelRegistry : public QObject {
        Q_OBJECT

    public:
        explicit ModelRegistry(std::shared_ptr<domain::repository::IModelRepository> repository, QObject *parent = nullptr);
        ~ModelRegistry() override = default;

        /**
         * @brief 初始化注册中心并从仓储加载数据
         */
        bool initialize(
            const QString &apiJsonPath = QStringLiteral(":/config/api.json"),
            const QString &modelsJsonPath = QStringLiteral(":/config/models.json")
        );

        // --- Providers ---
        QList<domain::model::ModelProvider> getActiveProviders() const;
        std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) const;
        void saveProvider(const domain::model::ModelProvider &provider);
        void deleteProvider(const QString &providerId);

        // --- Provider Models ---
        QList<domain::model::ProviderModel> getProviderModels(const QString &providerId) const;
        void saveProviderModel(const domain::model::ProviderModel &binding);
        void deleteProviderModel(const QString &providerId, const QString &remoteModelId);

        // --- Resolved Models ---
        QList<domain::model::ResolvedModel> getModelsForProvider(const QString &providerId) const;
        QList<domain::model::ResolvedModel> getEnabledResolvedModels() const;
        std::optional<domain::model::ResolvedModel> resolveModel(const QString &providerId, const QString &remoteModelId) const;

        // --- 兼容层 ---
        QList<domain::model::Model> getEnabledModels() const;
        std::optional<std::pair<domain::model::Model, domain::model::ModelProvider>> resolve(const QString &modelId) const;
        bool hasCapability(const QString &modelId, domain::model::ModelCapability cap) const;
        QList<domain::model::Model> hydrateDiscoveredModels(
            const QString &providerId,
            const QList<domain::model::Model> &discoveredModels) const;

        /**
         * @brief 探测并注册本地 Ollama 模型
         */
        void scanLocalOllamaModels(const QString &ollamaBaseUrl = QStringLiteral("http://localhost:11434"));

    signals:
        void providersChanged();
        void modelsChanged();

    private:
        void refreshCache();

        std::shared_ptr<domain::repository::IModelRepository> m_repository;

        QHash<QString, domain::model::ModelProvider> m_providers;
        QHash<QString, domain::model::CanonicalModel> m_canonicalModels;
        QHash<QString, QList<domain::model::ProviderModel>> m_providerModels;
        QList<domain::model::ResolvedModel> m_enabledResolvedModels;
    };

} // namespace core::model
