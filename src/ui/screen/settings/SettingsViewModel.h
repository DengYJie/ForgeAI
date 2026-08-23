#pragma once

#include "ui/base/BaseViewModel.h"
#include "domain/model/Model.h"
#include "domain/model/ModelProvider.h"
#include "application/usecase/settings/SettingsUseCases.h"
#include <QList>
#include <QString>

namespace ui::screen::settings {
    struct SettingsState {
        QList<domain::model::ModelProvider> providers;
        QList<domain::model::Model> enabledModels;
        QString statusMessage;

        bool operator==(const SettingsState &other) const = default;
    };

    /**
     * @brief 设置界面的 ViewModel，负责全局设置项与模型配置的协调流转
     */
    class SettingsViewModel : public BaseViewModel<SettingsViewModel, SettingsState> {
        Q_OBJECT

    public:
        explicit SettingsViewModel(
            const application::usecase::settings::SettingsUseCases &useCases = {},
            QObject *parent = nullptr
        );

        ~SettingsViewModel() override;

        /**
         * @brief 加载全部设置与模型数据
         */
        void loadAll();

        /**
         * @brief 保存全部设置
         */
        void saveAll();

        /**
         * @brief 刷新模型列表
         */
        void refreshModels();

        /**
         * @brief 打开模型与服务商管理对话框
         * @param parent 宿主父控件
         */
        void openModelManager(QWidget *parent = nullptr);

    Q_SIGNALS:
        void stateChanged(const ui::screen::settings::SettingsState &state);

    protected:
        void emitStateChanged() override;

    private:
        void setupUseCaseConnections();

        application::usecase::settings::SettingsUseCases m_useCases;
    };
} // namespace ui::screen::settings
