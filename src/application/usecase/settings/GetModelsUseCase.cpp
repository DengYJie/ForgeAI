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
                    this, &GetModelsUseCase::modelsChanged);
        }
    }

    QList<domain::model::ModelProvider> GetModelsUseCase::getActiveProviders() const {
        return m_modelService ? m_modelService->getActiveProviders() : QList<domain::model::ModelProvider>{};
    }

    QList<domain::model::Model> GetModelsUseCase::getEnabledModels() const {
        return m_modelService ? m_modelService->getEnabledModels() : QList<domain::model::Model>{};
    }
} // namespace application::usecase::settings
