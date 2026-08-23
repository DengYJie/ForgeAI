#include "SettingsService.h"
#include "core/settings/SettingsRegistry.h"

namespace services::settings {
    SettingsService::SettingsService(core::settings::SettingsRegistry *registry, QObject *parent)
        : ISettingsService(parent), m_registry(registry) {
    }

    void SettingsService::loadAll() {
        if (m_registry) {
            m_registry->loadAll();
        }
    }

    void SettingsService::saveAllSync() {
        if (m_registry) {
            m_registry->saveAllSync();
        }
    }
} // namespace services::settings
