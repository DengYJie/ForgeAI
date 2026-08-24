#pragma once
#include <QObject>
#include "core/settings/BaseSettingsProvider.h"
#include <FluentQt/Foundation.h>

namespace core::settings {
    /**
     * @brief 应用程序主题显示模式枚举
     */
    enum class ThemeMode {
        System = 0, ///< 跟随操作系统外观
        Light = 1,  ///< 强制浅色模式
        Dark = 2    ///< 强制深色模式
    };

    /**
     * @brief 外观与行为设置项持久化提供者
     * @details 维护应用主题模式并与操作系统配色方案联动
     */
    class AppearanceSettingsProvider : public BaseSettingsProvider {
        Q_OBJECT

    public:
        /**
         * @brief 主题显示模式配置键
         */
        static inline const SettingKey<ThemeMode> ThemeModeKey{"themeMode", ThemeMode::System};

        explicit AppearanceSettingsProvider(QObject *parent = nullptr);

        QString id() const override { return QStringLiteral("appearance"); }
        QString category() const override { return QStringLiteral("外观与行为"); }
        QString title() const override { return QStringLiteral("应用主题"); }
        bool useSeparateFile() const override { return false; }
        QString configFileName() const override { return QString(); }

    protected:
        void onSettingChanged(const QString &key) override;
        void onSettingsLoaded() override;

    private:
        void applyTheme() const;
        static fluent::FluentElement::Theme resolveSystemTheme();
    };
} // namespace core::settings
