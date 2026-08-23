#pragma once

#include "ui/window/NavigationWindow.h"
#include "application/usecase/chat/ChatUseCases.h"

namespace ui::screen::main {
    class MainViewModel;
    struct MainState;

    class MainWindow : public NavigationWindow {
        Q_OBJECT

    public:
        explicit MainWindow(
            const application::usecase::chat::ChatUseCases &chatUseCases = {},
            QWidget *parent = nullptr
        );

        ~MainWindow() override;

        MainViewModel *viewModel() const { return m_viewModel; }

    private:
        void setupUi();

        void setupViewModel();

        void setupConnections();

        void render(const MainState &state);

        application::usecase::chat::ChatUseCases m_chatUseCases;
        MainViewModel *m_viewModel = nullptr;
    };
} // namespace ui::screen::main
