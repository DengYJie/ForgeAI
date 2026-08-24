#include "GetModelsUseCase.h"
#include "domain/service/IModelService.h"

namespace application::usecase::settings {
    GetModelsUseCase::GetModelsUseCase(
        domain::service::IModelService *modelService,
        QObject *parent
    ) : QObject(parent),
        m_modelService(modelService) {
        if (m_modelService) {
            connect(m_modelService, &domain::service::IModelService::providersChanged,
                    this, [this] {
                emit providersChanged();
            });
            connect(m_modelService, &domain::service::IModelService::modelsChanged,
                    this, [this] {
                emit modelsChanged();
            });
        }
    }

    QList<domain::model::ModelProvider> GetModelsUseCase::getActiveProviders() const {
        return m_modelService ? m_modelService->getActiveProviders() : QList<domain::model::ModelProvider>{};
    }

    QList<domain::model::ModelProvider> GetModelsUseCase::getAllProviders() const {
        return m_modelService ? m_modelService->getAllProviders() : QList<domain::model::ModelProvider>{};
    }

    QList<domain::model::ResolvedModel> GetModelsUseCase::getModelsForProvider(const QString &providerId) const {
        return m_modelService ? m_modelService->getModelsForProvider(providerId) : QList<domain::model::ResolvedModel>{};
    }

    QList<domain::model::ResolvedModel> GetModelsUseCase::getEnabledResolvedModels() const {
        return m_modelService ? m_modelService->getEnabledResolvedModels() : QList<domain::model::ResolvedModel>{};
    }
} // namespace application::usecase::settings
