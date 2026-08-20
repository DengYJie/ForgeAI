#pragma once
#include "../ISettingsProvider.h"
#include <components/foundation/FluentElement.h>

namespace core::settings {
    class CoreSettingsProvider : public ISettingsProvider {
        Q_OBJECT

    public:
        enum class ThemeMode {
            System = 0,
            Light = 1,
            Dark = 2
        };

        Q_ENUM(ThemeMode)

        explicit CoreSettingsProvider(QObject *parent = nullptr);

        QString id() const override { return "core"; }
        QString category() const override { return "Appearance & behavior"; }
        bool useSeparateFile() const override { return false; }
        QString configFileName() const override { return ""; }

        void fromJson(const QJsonObject &json) override;

        void saveToJson(QJsonObject &json) const override;

        ThemeMode themeMode() const { return m_themeMode; }

        void setThemeMode(ThemeMode mode);

        void applyTheme();

    signals:
        void themeModeChanged(ThemeMode mode);

    private:
        fluent::FluentElement::Theme resolveSystemTheme() const;

        ThemeMode m_themeMode = ThemeMode::System;
    };
} // namespace core::settings
