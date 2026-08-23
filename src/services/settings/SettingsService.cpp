#include "SettingsService.h"
#include "core/settings/SettingsRegistry.h"

namespace services::settings {
    SettingsService::SettingsService(QObject *parent)
        : ISettingsService(parent) {
    }

    void SettingsService::loadAll() {
        core::settings::SettingsRegistry::instance().loadAll();
    }

    void SettingsService::saveAllSync() {
        core::settings::SettingsRegistry::instance().saveAllSync();
    }
} // namespace services::settings
