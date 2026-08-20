#pragma once

#include "ui/window/NavigationWindow.h"

namespace ui::screen::main {
    class MainViewModel;
    struct MainState;

    class MainWindow : public NavigationWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);

        ~MainWindow() override;

        MainViewModel *viewModel() const { return m_viewModel; }

    private:
        void setupUi();

        void setupViewModel();

        void setupConnections();

        void render(const MainState &state);

        MainViewModel *m_viewModel = nullptr;
    };
} // namespace ui::screen::main
