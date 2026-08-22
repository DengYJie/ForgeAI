#include "CoreSettingsProvider.h"
#include "core/settings/SettingsRegistry.h"
#include <QGuiApplication>
#include <QSettings>
#include <QPalette>
#include "compatibility/QtCompat.h"

namespace core::settings {
    CoreSettingsProvider::CoreSettingsProvider(QObject *parent) : BaseSettingsProvider(parent) {
        fluentConnectSystemColorSchemeChanged(this, [this]() {
            if (get(ThemeModeKey) == ThemeMode::System) {
                applyTheme();
            }
        });
    }

    fluent::FluentElement::Theme CoreSettingsProvider::resolveSystemTheme() {
        const FluentSystemColorScheme scheme = fluentSystemColorScheme();
        if (scheme == FluentSystemColorScheme::Dark)
            return fluent::FluentElement::Dark;
        if (scheme == FluentSystemColorScheme::Light)
            return fluent::FluentElement::Light;

#ifdef Q_OS_WIN
        const QSettings registry(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
            QSettings::NativeFormat);
        if (registry.contains(QStringLiteral("AppsUseLightTheme"))) {
            return registry.value(QStringLiteral("AppsUseLightTheme"), 1).toInt() == 0
                       ? fluent::FluentElement::Dark
                       : fluent::FluentElement::Light;
        }
#endif
        if (qApp) {
            const QPalette palette = qApp->palette();
            if (palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness()) {
                return fluent::FluentElement::Dark;
            }
        }
        return fluent::FluentElement::Dark;
    }

    void CoreSettingsProvider::onSettingChanged(const QString &key) {
        if (key == ThemeModeKey.name) {
            applyTheme();
        }
    }

    void CoreSettingsProvider::onSettingsLoaded() {
        applyTheme();
    }

    void CoreSettingsProvider::applyTheme() const {
        ThemeMode currentMode = get(ThemeModeKey);
        fluent::FluentElement::Theme effective = fluent::FluentElement::Dark;
        if (currentMode == ThemeMode::System) {
            effective = resolveSystemTheme();
        } else if (currentMode == ThemeMode::Light) {
            effective = fluent::FluentElement::Light;
        } else {
            effective = fluent::FluentElement::Dark;
        }
        fluent::FluentElement::setThemeDeferred(effective);
    }
} // namespace core::settings

REGISTER_SETTINGS_PROVIDER (core::settings::CoreSettingsProvider)
