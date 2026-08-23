#include "RefreshModelsUseCase.h"
#include "core/model/ModelRegistry.h"

namespace application::usecase::settings {

    RefreshModelsUseCase::RefreshModelsUseCase(
        ports::IModelDiscoveryGateway *discoveryGateway,
        std::shared_ptr<core::model::ModelRegistry> registry,
        QObject *parent
    ) : QObject(parent)
      , m_discoveryGateway(discoveryGateway)
      , m_registry(std::move(registry)) {
    }

    RefreshModelsUseCase::~RefreshModelsUseCase() {
        cancel();
    }

    void RefreshModelsUseCase::execute(const QString &providerId) {
        if (providerId.isEmpty() || !m_discoveryGateway || !m_registry) {
            emit discoveryFailed(providerId, "Discovery dependencies are not ready.");
            return;
        }

        auto optProvider = m_registry->getProvider(providerId);
        if (!optProvider.has_value()) {
            emit discoveryFailed(providerId, "Provider not found in registry.");
            return;
        }

        cancel();
        m_currentProviderId = providerId;
        emit discoveryStarted(providerId);

        m_currentOp = m_discoveryGateway->fetchModels(optProvider.value());
        if (m_currentOp) {
            m_currentOp->setParent(this);
            connect(m_currentOp, &ports::IModelDiscoveryOperation::finished, this, &RefreshModelsUseCase::onModelsFetched);
            connect(m_currentOp, &ports::IModelDiscoveryOperation::failed, this, &RefreshModelsUseCase::onFetchFailed);
        }
    }

    void RefreshModelsUseCase::cancel() {
        if (m_currentOp) {
            m_currentOp->cancel();
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
        m_currentProviderId.clear();
    }

    bool RefreshModelsUseCase::isDiscovering() const {
        return m_currentOp != nullptr;
    }

    void RefreshModelsUseCase::onModelsFetched(const QList<domain::model::Model> &models) {
        if (m_currentProviderId.isEmpty()) return;

        auto optProvider = m_registry->getProvider(m_currentProviderId);
        if (optProvider.has_value()) {
            auto provider = optProvider.value();
            // 优先与本地注册表元数据模板 (models.json) 与已有配置匹配补全
            auto hydratedModels = m_registry->hydrateDiscoveredModels(m_currentProviderId, models);
            provider.models = hydratedModels;
            m_registry->saveProvider(provider);

            emit discoveryFinished(m_currentProviderId, hydratedModels.size());
        } else {
            emit discoveryFailed(m_currentProviderId, "Provider removed during discovery.");
        }

        if (m_currentOp) {
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
        m_currentProviderId.clear();
    }

    void RefreshModelsUseCase::onFetchFailed(const QString &errorMessage) {
        if (m_currentProviderId.isEmpty()) return;

        emit discoveryFailed(m_currentProviderId, errorMessage);

        if (m_currentOp) {
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
        m_currentProviderId.clear();
    }

} // namespace application::usecase::settings
