#pragma once

#include <QObject>
#include <QList>

#include "domain/service/ISettingsService.h"

namespace application::usecase::settings {
    class GetSettingsProvidersUseCase : public QObject {
        Q_OBJECT

    public:
        explicit GetSettingsProvidersUseCase(
            domain::service::ISettingsService *settingsService,
            QObject *parent = nullptr
        );

        ~GetSettingsProvidersUseCase() override = default;

        QList<domain::service::SettingsProviderSummary> execute() const;

    private:
        domain::service::ISettingsService *m_settingsService = nullptr;
    };
} // namespace application::usecase::settings
