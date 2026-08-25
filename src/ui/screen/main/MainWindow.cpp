#include "MainWindow.h"

#include <FluentQt/Design.h>
#include <FluentQt/Navigation.h>
#include "MainViewModel.h"
#include "ui/navigation/NavigationController.h"
#include "ui/navigation/NavigationPanel.h"
#include "ui/screen/chat/ChatPage.h"
#include "ui/screen/work/WorkPage.h"
#include "ui/screen/knowledge/KnowledgePage.h"
#include "ui/screen/settings/SettingsNavigationModule.h"
#include "ui/window/TitleBar.h"

namespace ui::screen::main {
    MainWindow::MainWindow(
        MainViewModel *mainViewModel,
        ui::screen::chat::ChatViewModel *chatViewModel,
        ui::screen::work::WorkViewModel *workViewModel,
        ui::screen::knowledge::KnowledgeViewModel *knowledgeViewModel,
        ui::screen::settings::SettingsUIRegistry *settingsUiRegistry,
        QWidget *parent
    ) : NavigationWindow(parent),
        m_viewModel(mainViewModel),
        m_chatViewModel(chatViewModel),
        m_workViewModel(workViewModel),
        m_knowledgeViewModel(knowledgeViewModel),
        m_settingsUiRegistry(settingsUiRegistry),
        m_controller(new ui::navigation::NavigationController(this, this)) {
        setupUi();
        setupConnections();
    }

    MainWindow::~MainWindow() = default;

    void MainWindow::setupUi() {
        resize(1000, 680);
        setMinimumSize(640, 480);
        navigationView()->setExpandedPaneWidth(240);
        navigationView()->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);
        navigationView()->setPaneOpen(false);

        // 1. 注册顶部导航项：对话、工作、知识库 (注册在 "main" surface)
        auto *chatPage = new ui::screen::chat::ChatPage(m_chatViewModel, this);
        addSubInterface(
            QStringLiteral("chat"),
            chatPage,
            Typography::Icons::glyph(QStringLiteral("ic_fluent_chat_20_regular")),
            tr("对话"),
            QString(),
            ui::navigation::NavigationItemPosition::Top
        );

        auto *workPage = new ui::screen::work::WorkPage(m_workViewModel, this);
        addSubInterface(
            QStringLiteral("work"),
            workPage,
            Typography::Icons::glyph(QStringLiteral("ic_fluent_cursor_click_20_regular")),
            tr("工作"),
            QString(),
            ui::navigation::NavigationItemPosition::Top
        );

        // 2. 安装设置特性模块 (Feature Module Installer)
        m_settingsModule = new ui::screen::settings::SettingsNavigationModule(
            m_settingsUiRegistry,
            this
        );
        if (m_settingsModule->install(*this)) {
            // 3. 若安装成功且存在有效初始路由，将模块的入口项挂载到主导航栏
            if (const auto entry = m_settingsModule->entryDescriptor()) {
                addNavigationItem(*entry, QStringLiteral("main"));
            }
        }
    }

    void MainWindow::setupConnections() {
        // 1. 导航请求意图统一由 NavigationController 调度
        connect(this, &NavigationWindow::navigationRequested,
                m_controller, &ui::navigation::NavigationController::navigate);

        // 2. 返回请求统一由 NavigationController 决定
        connect(this, &NavigationWindow::backRequested,
                m_controller, &ui::navigation::NavigationController::goBack);

        // 3. 前进请求
        connect(this, &NavigationWindow::forwardRequested,
                m_controller, &ui::navigation::NavigationController::goForward);

        // 4. TitleBar 返回按钮可见性绑定
        if (titleBar()) {
            titleBar()->setBackButtonVisible(m_controller->canGoBack());
            connect(m_controller, &ui::navigation::NavigationController::canGoBackChanged,
                    titleBar(), &ui::window::TitleBar::setBackButtonVisible);
        }

        // 5. 初始导航
        qDebug().noquote() << "[MainWindow] initial navigation to chat";
        m_controller->navigate(QStringLiteral("chat"));
    }

    void MainWindow::resizeEvent(QResizeEvent *event) {
        NavigationWindow::resizeEvent(event);
        if (m_settingsModule) {
            m_settingsModule->updateResponsiveLayout(width());
        }
    }

} // namespace ui::screen::main
