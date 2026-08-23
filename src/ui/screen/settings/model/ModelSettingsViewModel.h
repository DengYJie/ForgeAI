#pragma once
#include <QObject>

namespace ui::screen::settings {
    /**
     * @brief 模型设置局部 ViewModel
     * @details 负责模型管理意图流转，与 QWidget/QDialog 完全解耦
     */
    class ModelSettingsViewModel : public QObject {
        Q_OBJECT

    public:
        explicit ModelSettingsViewModel(QObject *parent = nullptr);
        ~ModelSettingsViewModel() override = default;

        /**
         * @brief 发起打开模型与服务商管理器意图
         */
        void requestOpenModelManager();

    Q_SIGNALS:
        /**
         * @brief 请求打开模型管理器对话框信号
         */
        void modelManagerRequested();
    };
} // namespace ui::screen::settings
