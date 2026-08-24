#include "ModelRegistry.h"
#include <QDebug>

namespace core::model {

    namespace {
        QString unqualifiedModelId(QString modelId) {
            modelId = modelId.trimmed();
            while (modelId.endsWith(QLatin1Char('/'))) modelId.chop(1);
            const int separator = modelId.lastIndexOf(QLatin1Char('/'));
            return separator >= 0 ? modelId.mid(separator + 1) : modelId;
        }
    } // namespace

    ModelRegistry::ModelRegistry(std::shared_ptr<domain::repository::IModelRepository> repository, QObject *parent)
        : QObject(parent), m_repository(std::move(repository)) {
    }

    bool ModelRegistry::initialize(const QString &apiJsonPath, const QString &modelsJsonPath) {
        Q_UNUSED(apiJsonPath)
        Q_UNUSED(modelsJsonPath)

        refreshCache();

        qInfo().noquote() << QStringLiteral("[ModelRegistry] initialize 完成: 活跃服务商=%1, 启用模型=%2")
            .arg(m_providers.size()).arg(m_enabledResolvedModels.size());

        emit providersChanged();
        emit modelsChanged();
        return true;
    }

    void ModelRegistry::refreshCache() {
        m_providers.clear();
        m_enabledResolvedModels.clear();

        if (!m_repository) {
            return;
        }

        auto enabledProviders = m_repository->getEnabledProviders();
        for (const auto &p : enabledProviders) {
            m_providers.insert(p.id, p);
        }

        m_enabledResolvedModels = m_repository->getEnabledResolvedModels();
    }

    QList<domain::model::ModelProvider> ModelRegistry::getActiveProviders() const {
        return m_providers.values();
    }

    QList<domain::model::ModelProvider> ModelRegistry::getAllProviders() const {
        if (m_repository) {
            auto all = m_repository->getAllProviders();
            qInfo().noquote() << QStringLiteral("[ModelRegistry] getAllProviders (穿透仓储): %1 个").arg(all.size());
            return all;
        }
        return m_providers.values();
    }

    std::optional<domain::model::ModelProvider> ModelRegistry::getProvider(const QString &providerId) const {
        auto it = m_providers.find(providerId);
        if (it != m_providers.end()) {
            return it.value();
        }
        if (m_repository) {
            return m_repository->getProvider(providerId);
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

        if (m_repository) {
            m_repository->deleteProvider(providerId);
        }

        refreshCache();
        emit providersChanged();
        emit modelsChanged();
    }

    QList<domain::model::ProviderModel> ModelRegistry::getProviderModels(const QString &providerId) const {
        if (m_repository) {
            return m_repository->getProviderModels(providerId);
        }
        return {};
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
        for (const auto &rm : m_enabledResolvedModels) {
            if (rm.provider.id == providerId && rm.requestModelId() == remoteModelId) {
                return rm;
            }
        }
        if (m_repository) {
            return m_repository->resolveModel(providerId, remoteModelId);
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

    QList<domain::model::ProviderModel> ModelRegistry::hydrateDiscoveredModels(
        const QString &providerId,
        const QList<domain::model::ProviderModel> &discoveredModels) const {
        QList<domain::model::ProviderModel> result;
        const QList<domain::model::CanonicalModel> canonicalModels = m_repository
            ? m_repository->getAllCanonicalModels()
            : QList<domain::model::CanonicalModel>{};
        QHash<QString, QString> canonicalIds;
        for (const auto &canonical : canonicalModels) {
            canonicalIds.insert(canonical.id, canonical.id);
        }

        for (const auto &raw : discoveredModels) {
            domain::model::ProviderModel pm = raw;
            pm.providerId = providerId;
            pm.isCustom = false;
            pm.origin = domain::model::DataOrigin::Discovered;
            if (m_repository) {
                const auto exactMatch = canonicalIds.constFind(raw.remoteModelId);
                if (exactMatch != canonicalIds.cend()) {
                    pm.canonicalModelId = exactMatch.value();
                } else {
                    const QString unqualifiedId = unqualifiedModelId(raw.remoteModelId);
                    QList<domain::model::CanonicalModel> candidates;
                    for (const auto &canonical : canonicalModels) {
                        if (unqualifiedModelId(canonical.id).compare(unqualifiedId, Qt::CaseInsensitive) == 0) {
                            candidates.append(canonical);
                        }
                    }

                    // A suffix is safe only when it identifies one canonical
                    // model. Ambiguous names remain unhydrated rather than
                    // assigning metadata from the wrong provider/model.
                    if (candidates.size() == 1) {
                        pm.canonicalModelId = candidates.first().id;
                    }
                }
            }
            result.append(pm);
        }
        return result;
    }

    void ModelRegistry::scanLocalOllamaModels(const QString &ollamaBaseUrl) {
        Q_UNUSED(ollamaBaseUrl)
    }

} // namespace core::model
