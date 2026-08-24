#include "MainWindow.h"

#include <FluentQt/Design.h>
#include <FluentQt/Navigation.h>
#include "MainViewModel.h"
#include "ui/animation/AnimatedSettingsVisualSource.h"
#include "ui/navigation/NavigationPanel.h"
#include "ui/screen/chat/ChatPage.h"
#include "ui/screen/work/WorkPage.h"
#include "ui/screen/knowledge/KnowledgePage.h"
#include "ui/screen/settings/SettingsPage.h"
#include "ui/window/TitleBar.h"

namespace ui::screen::main {
    MainWindow::MainWindow(
        MainViewModel *mainViewModel,
        ui::screen::chat::ChatViewModel *chatViewModel,
        ui::screen::work::WorkViewModel *workViewModel,
        ui::screen::knowledge::KnowledgeViewModel *knowledgeViewModel,
        ui::screen::settings::SettingsViewModel *settingsViewModel,
        ui::screen::settings::SettingsUIRegistry *settingsUiRegistry,
        ui::screen::settings::SettingsCoordinator *settingsCoordinator,
        QWidget *parent
    ) : NavigationWindow(parent),
        m_viewModel(mainViewModel),
        m_chatViewModel(chatViewModel),
        m_workViewModel(workViewModel),
        m_knowledgeViewModel(knowledgeViewModel),
        m_settingsViewModel(settingsViewModel),
        m_settingsUiRegistry(settingsUiRegistry),
        m_settingsCoordinator(settingsCoordinator) {
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
        
        // 1. 注册顶部导航项：对话、工作、知识库
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

        auto *knowledgePage = new ui::screen::knowledge::KnowledgePage(m_knowledgeViewModel, this);
        addSubInterface(
            QStringLiteral("knowledge"),
            knowledgePage,
            Typography::Icons::glyph(QStringLiteral("ic_fluent_document_search_20_regular")),
            tr("知识库"),
            QString(),
            ui::navigation::NavigationItemPosition::Top
        );

        // 2. 注册底部设置按钮。settings 本身不再是内容页，只负责进入设置 provider 路由。
        auto *settingsRoutes = new ui::screen::settings::SettingsPage(
            m_settingsViewModel,
            m_settingsUiRegistry,
            m_settingsCoordinator,
            navigationView(),
            navigationPanel(),
            this
        );
        settingsRoutes->registerProviderRoutes(this);

        addSubInterface(
            QStringLiteral("settings"),
            nullptr,
            Typography::Icons::Settings,
            tr("设置"),
            QString(),
            ui::navigation::NavigationItemPosition::Bottom,
            false,
            std::make_shared<ui::animation::AnimatedSettingsVisualSource>()
        );
        connect(navigationPanel(), &ui::navigation::NavigationPanel::itemSelected,
                this, [this, settingsRoutes](const QString &routeKey) {
                    if (routeKey != QStringLiteral("settings")) return;

                    const QString initialRoute = settingsRoutes->initialProviderRouteKey();
                    if (!initialRoute.isEmpty()) {
                        switchTo(initialRoute);
                    }
                });

        switchTo(QStringLiteral("chat"));
    }

    void MainWindow::setupConnections() {
    }

} // namespace ui::screen::main
