#pragma once

#include "ui/window/NavigationWindow.h"

namespace ui::screen::chat {
    class ChatViewModel;
}

namespace ui::screen::main {
    class MainViewModel;
    struct MainState;

    class MainWindow : public NavigationWindow {
        Q_OBJECT

    public:
        explicit MainWindow(
            ui::screen::chat::ChatViewModel *chatViewModel = nullptr,
            QWidget *parent = nullptr
        );

        ~MainWindow() override;

        MainViewModel *viewModel() const { return m_viewModel; }

    private:
        void setupUi();

        void setupViewModel();

        void setupConnections();

        void render(const MainState &state);

        ui::screen::chat::ChatViewModel *m_chatViewModel = nullptr;
        MainViewModel *m_viewModel = nullptr;
    };
} // namespace ui::screen::main
