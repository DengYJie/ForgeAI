#include "SaveSettingUseCase.h"
#include "domain/service/ISettingsService.h"

namespace application::usecase::settings {
    SaveSettingUseCase::SaveSettingUseCase(
        domain::service::ISettingsService *settingsService,
        QObject *parent
    ) : QObject(parent),
        m_settingsService(settingsService) {
    }

    void SaveSettingUseCase::execute() {
        if (m_settingsService) {
            m_settingsService->saveAllSync();
        }
    }
} // namespace application::usecase::settings
