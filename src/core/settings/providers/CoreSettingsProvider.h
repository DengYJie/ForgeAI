#pragma once
#include <QObject>
#include "core/settings/BaseSettingsProvider.h"
#include <FluentQt/Foundation.h>

namespace core::settings {
    enum class ThemeMode {
        System = 0,
        Light = 1,
        Dark = 2
    };

    class CoreSettingsProvider : public BaseSettingsProvider {
        Q_OBJECT

    public:
        static inline const SettingKey<ThemeMode> ThemeModeKey{"themeMode", ThemeMode::System};

        explicit CoreSettingsProvider(QObject *parent = nullptr);

        QString id() const override { return "core"; }
        QString category() const override { return "Appearance & behavior"; }
        bool useSeparateFile() const override { return false; }
        QString configFileName() const override { return ""; }

    protected:
        void onSettingChanged(const QString &key) override;

        void onSettingsLoaded() override;

    private:
        void applyTheme() const;

        static fluent::FluentElement::Theme resolveSystemTheme();
    };
} // namespace core::settings
