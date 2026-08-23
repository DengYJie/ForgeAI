#pragma once

#include <QObject>

namespace domain::service {
    class ISettingsService;
}

namespace application::usecase::settings {
    /**
     * @brief 保存全局设置用例
     */
    class SaveSettingUseCase : public QObject {
        Q_OBJECT

    public:
        explicit SaveSettingUseCase(
            domain::service::ISettingsService *settingsService,
            QObject *parent = nullptr
        );

        ~SaveSettingUseCase() override = default;

        /**
         * @brief 执行设置同步落地
         */
        void execute();

    private:
        domain::service::ISettingsService *m_settingsService = nullptr;
    };
} // namespace application::usecase::settings
