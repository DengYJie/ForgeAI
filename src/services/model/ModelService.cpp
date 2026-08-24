#include "ModelService.h"
#include "core/model/ModelRegistry.h"

namespace services::model {
    ModelService::ModelService(
        std::shared_ptr<core::model::ModelRegistry> registry,
        QObject *parent
    ) : IModelService(parent),
        m_registry(std::move(registry)) {
        if (m_registry) {
            connect(m_registry.get(), &core::model::ModelRegistry::providersChanged,
                    this, &IModelService::providersChanged);
            connect(m_registry.get(), &core::model::ModelRegistry::modelsChanged,
                    this, &IModelService::modelsChanged);
        }
    }

    QList<domain::model::ModelProvider> ModelService::getActiveProviders() const {
        return m_registry ? m_registry->getActiveProviders() : QList<domain::model::ModelProvider>{};
    }

    QList<domain::model::ModelProvider> ModelService::getAllProviders() const {
        return m_registry ? m_registry->getAllProviders() : QList<domain::model::ModelProvider>{};
    }

    std::optional<domain::model::ModelProvider> ModelService::getProvider(const QString &providerId) const {
        return m_registry ? m_registry->getProvider(providerId) : std::nullopt;
    }

    void ModelService::saveProvider(const domain::model::ModelProvider &provider) {
        if (m_registry) {
            m_registry->saveProvider(provider);
        }
    }

    void ModelService::deleteProvider(const QString &providerId) {
        if (m_registry) {
            m_registry->deleteProvider(providerId);
        }
    }

    QList<domain::model::ResolvedModel> ModelService::getEnabledResolvedModels() const {
        return m_registry ? m_registry->getEnabledResolvedModels() : QList<domain::model::ResolvedModel>{};
    }

    QList<domain::model::ResolvedModel> ModelService::getModelsForProvider(const QString &providerId) const {
        return m_registry ? m_registry->getModelsForProvider(providerId) : QList<domain::model::ResolvedModel>{};
    }

    std::optional<domain::model::ResolvedModel> ModelService::resolveModel(const QString &providerId, const QString &remoteModelId) const {
        return m_registry ? m_registry->resolveModel(providerId, remoteModelId) : std::nullopt;
    }

    void ModelService::saveProviderModel(const domain::model::ProviderModel &binding) {
        if (m_registry) {
            m_registry->saveProviderModel(binding);
        }
    }

    void ModelService::deleteProviderModel(const QString &providerId, const QString &remoteModelId) {
        if (m_registry) {
            m_registry->deleteProviderModel(providerId, remoteModelId);
        }
    }
} // namespace services::model
