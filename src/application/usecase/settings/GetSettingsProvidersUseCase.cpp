#include "GetSettingsProvidersUseCase.h"

namespace application::usecase::settings {
    GetSettingsProvidersUseCase::GetSettingsProvidersUseCase(
        domain::service::ISettingsService *settingsService,
        QObject *parent
    ) : QObject(parent)
      , m_settingsService(settingsService) {
    }

    QList<domain::service::SettingsProviderSummary> GetSettingsProvidersUseCase::execute() const {
        return m_settingsService ? m_settingsService->providerSummaries() : QList<domain::service::SettingsProviderSummary>{};
    }
} // namespace application::usecase::settings
