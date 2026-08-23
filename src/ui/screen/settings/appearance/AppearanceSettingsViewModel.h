#pragma once
#include <QObject>
#include "core/settings/providers/AppearanceSettingsProvider.h"

namespace ui::screen::settings {
    /**
     * @brief 外观与行为设置项局部 ViewModel
     */
    class AppearanceSettingsViewModel : public QObject {
        Q_OBJECT

    public:
        /**
         * @param provider 外观设置持久化提供者指针
         * @param parent 父 QObject
         */
        explicit AppearanceSettingsViewModel(
            core::settings::AppearanceSettingsProvider *provider,
            QObject *parent = nullptr
        );
        ~AppearanceSettingsViewModel() override = default;

        /**
         * @brief 获取当前主题模式
         */
        core::settings::ThemeMode themeMode() const;

        /**
         * @brief 设置主题模式
         * @param mode 目标主题模式
         */
        void setThemeMode(core::settings::ThemeMode mode);

    Q_SIGNALS:
        /**
         * @brief 主题模式变更信号
         * @param mode 新的主题模式
         */
        void themeModeChanged(core::settings::ThemeMode mode);

    private:
        core::settings::AppearanceSettingsProvider *m_provider = nullptr;
    };
} // namespace ui::screen::settings
