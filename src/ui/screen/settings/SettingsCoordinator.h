#pragma once
#include <QObject>
#include <QString>
#include <QWidget>

namespace ui::screen::settings::model_manager {
    class ModelManagerViewModel;
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
         * @param viewModel 模型管理器 ViewModel
         * @param parent 父 QObject
         */
        explicit SettingsCoordinator(
            model_manager::ModelManagerViewModel *viewModel,
            QObject *parent = nullptr
        );

        ~SettingsCoordinator() override = default;

        /**
         * @brief 请求激活模型与服务商 provider 页面
         * @param parent 兼容旧入口的宿主控件参数，当前不再创建弹窗
         */
        void openModelManager(QWidget *parent = nullptr);

    signals:
        void providerPageRequested(const QString &providerId);

    private:
        model_manager::ModelManagerViewModel *m_viewModel = nullptr;
    };
} // namespace ui::screen::settings
