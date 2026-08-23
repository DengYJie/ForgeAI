#include "AppearanceSettingsViewModel.h"

namespace ui::screen::settings {
    AppearanceSettingsViewModel::AppearanceSettingsViewModel(
        core::settings::AppearanceSettingsProvider *provider,
        QObject *parent
    ) : QObject(parent), m_provider(provider) {
        if (m_provider) {
            connect(m_provider, &core::settings::ISettingsProvider::dataChanged, this, [this]() {
                emit themeModeChanged(themeMode());
            });
        }
    }

    core::settings::ThemeMode AppearanceSettingsViewModel::themeMode() const {
        if (!m_provider) {
            return core::settings::ThemeMode::System;
        }
        return m_provider->get(core::settings::AppearanceSettingsProvider::ThemeModeKey);
    }

    void AppearanceSettingsViewModel::setThemeMode(core::settings::ThemeMode mode) {
        if (m_provider) {
            m_provider->set(core::settings::AppearanceSettingsProvider::ThemeModeKey, mode);
        }
    }
} // namespace ui::screen::settings
