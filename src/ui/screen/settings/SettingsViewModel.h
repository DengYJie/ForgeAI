#pragma once

#include "ui/base/BaseViewModel.h"
#include "application/usecase/settings/SettingsUseCases.h"
#include <QString>

namespace ui::screen::settings {
    /**
     * @brief 设置主界面页面级状态载体
     */
    struct SettingsState {
        bool isLoading = false;       ///< 是否正处于设置加载状态
        QString statusMessage;        ///< 页面状态栏/Toast 提示信息文本

        bool operator==(const SettingsState &other) const = default;
    };

    /**
     * @brief 设置主界面页面级 ViewModel
     * @details 负责页面级生命周期、加载/保存编排与状态反馈，纯业务无 UI 依赖
     */
    class SettingsViewModel : public BaseViewModel<SettingsViewModel, SettingsState> {
        Q_OBJECT

    public:
        /**
         * @param useCases 设置相关业务用例聚合结构体
         * @param parent 父 QObject
         */
        explicit SettingsViewModel(
            const application::usecase::settings::SettingsUseCases &useCases = {},
            QObject *parent = nullptr
        );

        ~SettingsViewModel() override;

        /**
         * @brief 加载全部设置数据
         */
        void loadAll();

        /**
         * @brief 保存全部设置数据
         */
        void saveAll();

        /**
         * @brief 设置提示信息
         * @param message 提示文本
         */
        void setStatusMessage(const QString &message);

    Q_SIGNALS:
        /**
         * @brief 页面级状态变更信号
         * @param state 最新的页面状态
         */
        void stateChanged(const ui::screen::settings::SettingsState &state);

    protected:
        void emitStateChanged() override;

    private:
        application::usecase::settings::SettingsUseCases m_useCases;
    };
} // namespace ui::screen::settings
