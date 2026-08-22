#include "ChatPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ui/widget/CollapsibleSplitView.h"
#include "ChatSidebar.h"
#include "ChatHeader.h"
#include "ChatAnchorBar.h"
#include "ChatInputBox.h"
#include "ChatViewModel.h"

namespace ui::screen::chat {
    ChatPage::ChatPage(QWidget *parent)
        : BasePage(parent) {
        setupUi();
        setupViewModel();
    }

    void ChatPage::setupUi() {
        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(0, 0, 0, 0);
        m_rootLayout->setSpacing(0);

        m_splitView = new ui::widget::CollapsibleSplitView(this);
        m_rootLayout->addWidget(m_splitView);

        // 1. 左侧会话列表侧边栏 (可折叠面板，最小 200px，最大 380px，默认 260px)
        m_sidebar = new ChatSidebar(this);

        fluent::collections::SplitViewPaneOptions sidebarOptions;
        sidebarOptions.minimumSize = 200;
        sidebarOptions.maximumSize = 380;
        sidebarOptions.preferredSize = 260;

        m_splitView->addCollapsiblePane(
            m_sidebar,
            ui::widget::SplitPaneDisplayMode::Inline,
            /*compactLength=*/0,
            /*startExpanded=*/true,
            /*initialOpenLength=*/260,
            sidebarOptions
        );

        connect(m_sidebar, &ChatSidebar::newChatRequested, this, [this]() {
            static int sessionCounter = 1;
            const QString newId = QStringLiteral("session_%1").arg(++sessionCounter);
            const QString title = tr("新对话 %1").arg(sessionCounter);
            m_sidebar->addSession(newId, title);
            m_sidebar->selectSession(newId);
        });

        // 2. 右侧主对话工作区 (第二 Pane)
        m_chatAreaWidget = new QWidget(this);
        auto *chatAreaLayout = new QVBoxLayout(m_chatAreaWidget);
        chatAreaLayout->setContentsMargins(0, 0, 0, 16);
        chatAreaLayout->setSpacing(0);

        // 2.1 顶部栏 (四段式: left/title/center/right)
        m_header = new ChatHeader(m_chatAreaWidget);
        m_header->setTitle(tr("新对话"));
        connect(m_header, &ChatHeader::toggleSidebarRequested, this, [this]() {
            m_splitView->togglePane(0, true);
            m_header->setSidebarExpanded(m_splitView->isPaneExpanded(0));
        });
        chatAreaLayout->addWidget(m_header);

        // 2.2 中部三列区域（左侧对话锚点 + 中央主内容占位 + 右侧占位）
        auto *middleRow = new QWidget(m_chatAreaWidget);
        auto *middleLayout = new QHBoxLayout(middleRow);
        middleLayout->setContentsMargins(4, 6, 8, 6);
        middleLayout->setSpacing(8);

        // 左侧对话锚点条
        m_anchorBar = new ChatAnchorBar(middleRow);
        middleLayout->addWidget(m_anchorBar, 0);

        // 中央主区域（占位 QWidget）
        m_mainArea = new QWidget(middleRow);
        m_mainArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        middleLayout->addWidget(m_mainArea, 1);

        // 右侧区域（先空着）
        m_mainRight = new QWidget(middleRow);
        m_mainRight->setFixedWidth(0);
        middleLayout->addWidget(m_mainRight, 0);

        chatAreaLayout->addWidget(middleRow, 1);

        // 2.3 底部输入框容器（带悬浮安全边距与水平居中）
        auto *inputContainer = new QWidget(m_chatAreaWidget);
        auto *inputLayout = new QHBoxLayout(inputContainer);
        inputLayout->setContentsMargins(20, 0, 20, 0);
        inputLayout->setSpacing(0);
        inputLayout->setAlignment(Qt::AlignHCenter);

        m_inputBox = new ChatInputBox(inputContainer);
        inputLayout->addWidget(m_inputBox);

        chatAreaLayout->addWidget(inputContainer);

        m_anchorBar->addAnchor(QStringLiteral("a1"), tr("构建配置讨论"), tr("关于 CMakeLists 结构优化及库依赖链接的配置建议。"));
        m_anchorBar->addAnchor(QStringLiteral("a2"), tr("UI 设计方案"), tr("主界面布局体系、侧边栏折叠及 FluentQt 组件选型。"));
        m_anchorBar->addAnchor(QStringLiteral("a3"), tr("代码落地实施"),
                               tr("完成了 ChatHeader、ChatAnchorBar 和 ChatInputBox 的全部代码实现。"));
        m_anchorBar->setActiveIndex(2);

        connect(m_inputBox, &ChatInputBox::sendRequested, this, [this](const QString &text) {
            static int anchorId = 3;
            m_anchorBar->addAnchor(QStringLiteral("a%1").arg(++anchorId), text.left(12), text);
            m_anchorBar->setActiveIndex(m_anchorBar->count() - 1);
        });

        fluent::collections::SplitViewPaneOptions chatPaneOptions;
        chatPaneOptions.fill = true;
        chatPaneOptions.minimumSize = 300;
        m_splitView->addPane(m_chatAreaWidget, chatPaneOptions);
    }

    void ChatPage::setupViewModel() {
        m_viewModel = new ChatViewModel(this);
        m_viewModel->observe(this, &ChatPage::render);
    }

    void ChatPage::render(const ChatState &state) {
        Q_UNUSED(state);
    }
} // namespace ui::screen::chat
