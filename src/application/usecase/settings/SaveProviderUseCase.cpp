#include "SaveProviderUseCase.h"
#include "domain/service/IModelService.h"

namespace application::usecase::settings {

    SaveProviderUseCase::SaveProviderUseCase(
        domain::service::IModelService *modelService,
        QObject *parent
    ) : QObject(parent),
        m_modelService(modelService) {
    }

    void SaveProviderUseCase::execute(const domain::model::ModelProvider &provider) {
        if (m_modelService) {
            m_modelService->saveProvider(provider);
        }
    }

} // namespace application::usecase::settings
