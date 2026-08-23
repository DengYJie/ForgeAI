#pragma once

#include <QObject>

namespace domain::service {
    class ISettingsService;
}

namespace application::usecase::settings {
    /**
     * @brief 加载全局设置用例
     */
    class LoadSettingsUseCase : public QObject {
        Q_OBJECT

    public:
        explicit LoadSettingsUseCase(
            domain::service::ISettingsService *settingsService,
            QObject *parent = nullptr
        );

        ~LoadSettingsUseCase() override = default;

        /**
         * @brief 执行全量设置加载
         */
        void execute();

    private:
        domain::service::ISettingsService *m_settingsService = nullptr;
    };
} // namespace application::usecase::settings
