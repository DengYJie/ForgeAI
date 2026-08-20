#include "MainWindow.h"

#include <FluentQt/Design.h>
#include <FluentQt/Navigation.h>
#include "MainViewModel.h"
#include "ui/animation/AnimatedSettingsVisualSource.h"
#include "ui/navigation/NavigationPanel.h"
#include "ui/screen/settings/SettingsPage.h"
#include "ui/window/TitleBar.h"

namespace ui::screen::main {
    MainWindow::MainWindow(QWidget *parent)
        : NavigationWindow(parent) {
        setupUi();
        setupViewModel();
        setupConnections();
    }

    MainWindow::~MainWindow() = default;

    void MainWindow::setupUi() {
        resize(1000, 680);
        setMinimumSize(640, 480);

        // 1. 注册顶部主页面 (Home)
        auto *homePage = new QWidget(this);
        addSubInterface(
            QStringLiteral("home"),
            homePage,
            Typography::Icons::Home,
            tr("Home"),
            QString(),
            ui::navigation::NavigationItemPosition::Top
        );

        // 2. 注册底部设置项 (Settings)，采用 Fluent SettingsPage 并绑定旋转矢量动画
        auto *settingsPage = new ui::screen::settings::SettingsPage(this);
        addSubInterface(
            QStringLiteral("settings"),
            settingsPage,
            Typography::Icons::Settings,
            tr("Settings"),
            QString(),
            ui::navigation::NavigationItemPosition::Bottom,
            true,
            std::make_shared<ui::animation::AnimatedSettingsVisualSource>()
        );
    }

    void MainWindow::setupViewModel() {
        m_viewModel = new MainViewModel(this);
        m_viewModel->observe(this, &MainWindow::render);
    }

    void MainWindow::setupConnections() {
        if (!m_viewModel) return;

        // 导航面板项选择 -> 同步通知 ViewModel
        if (m_panel) {
            connect(m_panel, &ui::navigation::NavigationPanel::itemSelected,
                    m_viewModel, &MainViewModel::navigateTo);
        }
    }

    void MainWindow::render(const MainState &state) {
        // 同步路由切换
        if (!state.currentRoute.isEmpty()) {
            switchTo(state.currentRoute);
        }
    }
} // namespace ui::screen::main
