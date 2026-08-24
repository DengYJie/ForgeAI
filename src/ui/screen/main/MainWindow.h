#pragma once

#include "ui/window/NavigationWindow.h"

namespace ui::screen::chat {
    class ChatViewModel;
}

namespace ui::screen::work {
    class WorkViewModel;
}

namespace ui::screen::knowledge {
    class KnowledgeViewModel;
}

namespace ui::navigation {
    class NavigationController;
}

namespace ui::screen::settings {
    class SettingsNavigationModule;
    class SettingsUIRegistry;
}

namespace ui::screen::main {
    class MainViewModel;
    struct MainState;

    /**
     * @brief 主窗口，接收所有子界面的 ViewModels 并分发组装导航
     */
    class MainWindow : public NavigationWindow {
        Q_OBJECT

    public:
        explicit MainWindow(
            MainViewModel *mainViewModel = nullptr,
            ui::screen::chat::ChatViewModel *chatViewModel = nullptr,
            ui::screen::work::WorkViewModel *workViewModel = nullptr,
            ui::screen::knowledge::KnowledgeViewModel *knowledgeViewModel = nullptr,
            ui::screen::settings::SettingsUIRegistry *settingsUiRegistry = nullptr,
            QWidget *parent = nullptr
        );

        ~MainWindow() override;

        MainViewModel *viewModel() const { return m_viewModel; }
        ui::navigation::NavigationController *controller() const { return m_controller; }

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private:
        void setupUi();
        void setupConnections();

        MainViewModel *m_viewModel = nullptr;
        ui::screen::chat::ChatViewModel *m_chatViewModel = nullptr;
        ui::screen::work::WorkViewModel *m_workViewModel = nullptr;
        ui::screen::knowledge::KnowledgeViewModel *m_knowledgeViewModel = nullptr;
        ui::screen::settings::SettingsUIRegistry *m_settingsUiRegistry = nullptr;

        ui::navigation::NavigationController *m_controller = nullptr;
        ui::screen::settings::SettingsNavigationModule *m_settingsModule = nullptr;
    };
} // namespace ui::screen::main
