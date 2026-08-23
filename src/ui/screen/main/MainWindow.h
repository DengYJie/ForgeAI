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

namespace ui::screen::settings {
    class SettingsViewModel;
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
            ui::screen::settings::SettingsViewModel *settingsViewModel = nullptr,
            QWidget *parent = nullptr
        );

        ~MainWindow() override;

        MainViewModel *viewModel() const { return m_viewModel; }

    private:
        void setupUi();

        void setupConnections();

        void render(const MainState &state);

        MainViewModel *m_viewModel = nullptr;
        ui::screen::chat::ChatViewModel *m_chatViewModel = nullptr;
        ui::screen::work::WorkViewModel *m_workViewModel = nullptr;
        ui::screen::knowledge::KnowledgeViewModel *m_knowledgeViewModel = nullptr;
        ui::screen::settings::SettingsViewModel *m_settingsViewModel = nullptr;
    };
} // namespace ui::screen::main
