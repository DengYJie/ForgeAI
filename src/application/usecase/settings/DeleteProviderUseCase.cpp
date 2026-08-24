#include "DeleteProviderUseCase.h"
#include "domain/service/IModelService.h"

namespace application::usecase::settings {

    DeleteProviderUseCase::DeleteProviderUseCase(
        domain::service::IModelService *modelService,
        QObject *parent
    ) : QObject(parent),
        m_modelService(modelService) {
    }

    void DeleteProviderUseCase::execute(const QString &providerId) {
        if (m_modelService) {
            m_modelService->deleteProvider(providerId);
        }
    }

} // namespace application::usecase::settings
