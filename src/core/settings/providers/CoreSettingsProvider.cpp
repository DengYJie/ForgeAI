#include "CoreSettingsProvider.h"
#include "core/settings/SettingsRegistry.h"
#include <QGuiApplication>
#include <QStyleHints>
#include <QSettings>
#include <QPalette>
#include "compatibility/QtCompat.h"

namespace core::settings {
    CoreSettingsProvider::CoreSettingsProvider(QObject *parent) : ISettingsProvider(parent) {
        fluentConnectSystemColorSchemeChanged(this, [this]() {
            if (m_themeMode == ThemeMode::System) {
                applyTheme();
            }
        });
    }

    fluent::FluentElement::Theme CoreSettingsProvider::resolveSystemTheme() const {
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

    void CoreSettingsProvider::fromJson(const QJsonObject &json) {
        if (json.contains("themeMode")) {
            int val = json.value("themeMode").toInt(static_cast<int>(ThemeMode::System));
            m_themeMode = static_cast<ThemeMode>(val);
        }
        applyTheme();
        emit themeModeChanged(m_themeMode);
    }

    void CoreSettingsProvider::saveToJson(QJsonObject &json) const {
        json.insert("themeMode", static_cast<int>(m_themeMode));
    }

    void CoreSettingsProvider::setThemeMode(ThemeMode mode) {
        if (m_themeMode != mode) {
            m_themeMode = mode;
            applyTheme();
            emit themeModeChanged(m_themeMode);
            emit dataChanged();
        }
    }

    void CoreSettingsProvider::applyTheme() {
        fluent::FluentElement::Theme effective = fluent::FluentElement::Dark;
        if (m_themeMode == ThemeMode::System) {
            effective = resolveSystemTheme();
        } else if (m_themeMode == ThemeMode::Light) {
            effective = fluent::FluentElement::Light;
        } else {
            effective = fluent::FluentElement::Dark;
        }
        fluent::FluentElement::setThemeDeferred(effective);
    }
} // namespace core::settings

REGISTER_SETTINGS_PROVIDER(core::settings::CoreSettingsProvider)
