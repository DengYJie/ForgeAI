#include "ModelRegistry.h"
#include <QDebug>

namespace core::model {

    ModelRegistry::ModelRegistry(std::shared_ptr<domain::repository::IModelRepository> repository, QObject *parent)
        : QObject(parent), m_repository(std::move(repository)) {
    }

    bool ModelRegistry::initialize(const QString &apiJsonPath, const QString &modelsJsonPath) {
        Q_UNUSED(apiJsonPath)
        Q_UNUSED(modelsJsonPath)

        refreshCache();

        emit providersChanged();
        emit modelsChanged();
        return true;
    }

    void ModelRegistry::refreshCache() {
        m_providers.clear();
        m_canonicalModels.clear();
        m_providerModels.clear();
        m_enabledResolvedModels.clear();

        if (!m_repository) {
            return;
        }

        auto providers = m_repository->getAllProviders();
        for (const auto &p : providers) {
            m_providers.insert(p.id, p);
            m_providerModels.insert(p.id, m_repository->getProviderModels(p.id));
        }

        auto canonicals = m_repository->getAllCanonicalModels();
        for (const auto &cm : canonicals) {
            m_canonicalModels.insert(cm.id, cm);
        }

        m_enabledResolvedModels = m_repository->getEnabledResolvedModels();
    }

    QList<domain::model::ModelProvider> ModelRegistry::getActiveProviders() const {
        return m_providers.values();
    }

    std::optional<domain::model::ModelProvider> ModelRegistry::getProvider(const QString &providerId) const {
        auto it = m_providers.find(providerId);
        if (it != m_providers.end()) {
            return it.value();
        }
        return std::nullopt;
    }

    void ModelRegistry::saveProvider(const domain::model::ModelProvider &provider) {
        m_providers.insert(provider.id, provider);

        if (m_repository) {
            m_repository->saveProvider(provider);
        }

        refreshCache();
        emit providersChanged();
        emit modelsChanged();
    }

    void ModelRegistry::deleteProvider(const QString &providerId) {
        auto it = m_providers.find(providerId);
        if (it == m_providers.end() || it->origin == domain::model::DataOrigin::BuiltIn) {
            return;
        }

        m_providers.remove(providerId);
        m_providerModels.remove(providerId);

        if (m_repository) {
            m_repository->deleteProvider(providerId);
        }

        refreshCache();
        emit providersChanged();
        emit modelsChanged();
    }

    QList<domain::model::ProviderModel> ModelRegistry::getProviderModels(const QString &providerId) const {
        return m_providerModels.value(providerId);
    }

    void ModelRegistry::saveProviderModel(const domain::model::ProviderModel &binding) {
        if (m_repository) {
            m_repository->saveProviderModel(binding);
        }

        refreshCache();
        emit modelsChanged();
    }

    void ModelRegistry::deleteProviderModel(const QString &providerId, const QString &remoteModelId) {
        if (m_repository) {
            m_repository->deleteProviderModel(providerId, remoteModelId);
        }

        refreshCache();
        emit modelsChanged();
    }

    QList<domain::model::ResolvedModel> ModelRegistry::getModelsForProvider(const QString &providerId) const {
        if (m_repository) {
            return m_repository->getResolvedModelsForProvider(providerId);
        }
        return {};
    }

    QList<domain::model::ResolvedModel> ModelRegistry::getEnabledResolvedModels() const {
        return m_enabledResolvedModels;
    }

    std::optional<domain::model::ResolvedModel> ModelRegistry::resolveModel(const QString &providerId, const QString &remoteModelId) const {
        if (m_repository) {
            return m_repository->resolveModel(providerId, remoteModelId);
        }
        return std::nullopt;
    }

    QList<domain::model::Model> ModelRegistry::getEnabledModels() const {
        QList<domain::model::Model> list;
        list.reserve(m_enabledResolvedModels.size());
        for (const auto &rm : m_enabledResolvedModels) {
            list.append(domain::model::Model::fromResolved(rm));
        }
        return list;
    }

    std::optional<std::pair<domain::model::Model, domain::model::ModelProvider>> ModelRegistry::resolve(const QString &modelId) const {
        for (const auto &rm : m_enabledResolvedModels) {
            if (rm.requestModelId() == modelId) {
                return std::make_pair(domain::model::Model::fromResolved(rm), rm.provider);
            }
        }
        if (m_repository) {
            auto all = m_repository->getAllResolvedModels();
            for (const auto &rm : all) {
                if (rm.requestModelId() == modelId) {
                    return std::make_pair(domain::model::Model::fromResolved(rm), rm.provider);
                }
            }
        }
        return std::nullopt;
    }

    bool ModelRegistry::hasCapability(const QString &modelId, domain::model::ModelCapability cap) const {
        for (const auto &rm : m_enabledResolvedModels) {
            if (rm.requestModelId() == modelId) {
                return rm.effectiveCapabilities().testFlag(cap);
            }
        }
        return false;
    }

    QList<domain::model::Model> ModelRegistry::hydrateDiscoveredModels(
        const QString &providerId,
        const QList<domain::model::Model> &discoveredModels) const {
        QList<domain::model::Model> result;
        for (const auto &raw : discoveredModels) {
            domain::model::Model m = raw;
            m.providerId = providerId;
            if (m_canonicalModels.contains(raw.id)) {
                const auto &cm = m_canonicalModels.value(raw.id);
                m.displayName = cm.name;
                m.description = cm.description;
                m.family = cm.family;
                m.limits = cm.limits;
                m.capabilities = cm.capabilities;
                m.defaultParams = cm.defaultParams;
                m.openWeights = cm.openWeights;
                m.knowledgeCutoff = cm.knowledgeCutoff;
            }
            result.append(m);
        }
        return result;
    }

    void ModelRegistry::scanLocalOllamaModels(const QString &ollamaBaseUrl) {
        Q_UNUSED(ollamaBaseUrl)
    }

} // namespace core::model
