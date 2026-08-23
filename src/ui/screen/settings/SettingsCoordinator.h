#pragma once
#include <QObject>
#include <QWidget>

namespace core::model {
    class ModelRegistry;
}

namespace application::usecase::settings {
    class RefreshModelsUseCase;
}

namespace ui::screen::settings {
    /**
     * @brief 设置模块表现层协调者 (Presentation Coordinator)
     * @details 负责设置页面的子窗口、Dialog 弹出和跨界面导航生命周期协调，保持 ViewModel 与 QWidget 解耦
     */
    class SettingsCoordinator : public QObject {
        Q_OBJECT

    public:
        /**
         * @param modelRegistry 模型注册中心指针
         * @param refreshUseCase 模型探测刷新业务用例指针
         * @param parent 父 QObject
         */
        explicit SettingsCoordinator(
            core::model::ModelRegistry *modelRegistry,
            application::usecase::settings::RefreshModelsUseCase *refreshUseCase,
            QObject *parent = nullptr
        );

        ~SettingsCoordinator() override = default;

        /**
         * @brief 模态弹出模型与服务商管理对话框 (ModelManagerDialog)
         * @param parent 宿主窗口或父控件
         */
        void openModelManager(QWidget *parent = nullptr);

    private:
        core::model::ModelRegistry *m_modelRegistry = nullptr;
        application::usecase::settings::RefreshModelsUseCase *m_refreshUseCase = nullptr;
    };
} // namespace ui::screen::settings
