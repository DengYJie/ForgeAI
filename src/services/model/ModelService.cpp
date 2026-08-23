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
        }
    }

    QList<domain::model::ModelProvider> ModelService::getActiveProviders() const {
        return m_registry ? m_registry->getActiveProviders() : QList<domain::model::ModelProvider>{};
    }

    QList<domain::model::Model> ModelService::getEnabledModels() const {
        return m_registry ? m_registry->getEnabledModels() : QList<domain::model::Model>{};
    }

    std::optional<domain::model::ModelProvider> ModelService::getProvider(const QString &providerId) const {
        return m_registry ? m_registry->getProvider(providerId) : std::nullopt;
    }

    std::optional<std::pair<domain::model::Model, domain::model::ModelProvider>> ModelService::resolveModel(const QString &modelId) const {
        return m_registry ? m_registry->resolve(modelId) : std::nullopt;
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
} // namespace services::model
