#include "LoadSettingsUseCase.h"
#include "domain/service/ISettingsService.h"

namespace application::usecase::settings {
    LoadSettingsUseCase::LoadSettingsUseCase(
        domain::service::ISettingsService *settingsService,
        QObject *parent
    ) : QObject(parent),
        m_settingsService(settingsService) {
    }

    void LoadSettingsUseCase::execute() {
        if (m_settingsService) {
            m_settingsService->loadAll();
        }
    }
} // namespace application::usecase::settings
