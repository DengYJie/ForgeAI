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

    QList<domain::service::SettingsProviderSummary> SettingsService::providerSummaries() const {
        QList<domain::service::SettingsProviderSummary> result;
        if (!m_registry) return result;

        const auto providers = m_registry->allProviders();
        for (auto it = providers.cbegin(); it != providers.cend(); ++it) {
            if (!it.value()) continue;
            result.append({
                it.value()->id(),
                it.value()->category(),
                it.value()->title()
            });
        }
        return result;
    }
} // namespace services::settings
