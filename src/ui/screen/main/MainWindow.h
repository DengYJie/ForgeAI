#pragma once

#include "ui/window/NavigationWindow.h"

namespace domain::service {
    class IChatService;
    class IConversationService;
}

namespace ui::screen::main {
    class MainViewModel;
    struct MainState;

    class MainWindow : public NavigationWindow {
        Q_OBJECT

    public:
        explicit MainWindow(
            domain::service::IChatService *chatService = nullptr,
            domain::service::IConversationService *conversationService = nullptr,
            QWidget *parent = nullptr
        );

        ~MainWindow() override;

        MainViewModel *viewModel() const { return m_viewModel; }

    private:
        void setupUi();

        void setupViewModel();

        void setupConnections();

        void render(const MainState &state);

        domain::service::IChatService *m_chatService = nullptr;
        domain::service::IConversationService *m_conversationService = nullptr;
        MainViewModel *m_viewModel = nullptr;
    };
} // namespace ui::screen::main
