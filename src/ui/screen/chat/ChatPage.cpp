#include "ChatPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ui/widget/CollapsibleSplitView.h"
#include "ui/widget/chat/ChatHeader.h"
#include "ui/widget/chat/ChatAnchorBar.h"
#include "ui/widget/chat/ChatInputBox.h"
#include "ui/widget/message/MessageListView.h"
#include "ChatSidebar.h"
#include "ChatViewModel.h"

namespace ui::screen::chat {
    using namespace ui::widget::chat;
    using namespace ui::widget::message;

    ChatPage::ChatPage(
        const application::usecase::chat::ChatUseCases &useCases,
        QWidget *parent
    ) : BasePage(parent),
        m_useCases(useCases) {
        setupViewModel();
        setupUi();
    }

    void ChatPage::setupViewModel() {
        m_viewModel = new ChatViewModel(m_useCases, this);
    }

    void ChatPage::setupUi() {
        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(0, 0, 0, 0);
        m_rootLayout->setSpacing(0);

        m_splitView = new ui::widget::CollapsibleSplitView(this);
        m_rootLayout->addWidget(m_splitView);

        // 1. 左侧会话列表侧边栏 (可折叠面板)
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

        connect(m_sidebar, &ChatSidebar::newChatRequested, m_viewModel, &ChatViewModel::newSession);
        connect(m_sidebar, &ChatSidebar::sessionSelected, m_viewModel, &ChatViewModel::loadSession);
        connect(m_sidebar, &ChatSidebar::sessionDeleted, m_viewModel, &ChatViewModel::deleteSession);

        // 2. 右侧主对话工作区 (第二 Pane)
        m_chatAreaWidget = new QWidget(this);
        auto *chatAreaLayout = new QVBoxLayout(m_chatAreaWidget);
        chatAreaLayout->setContentsMargins(0, 0, 0, 16);
        chatAreaLayout->setSpacing(0);

        // 2.1 顶部栏
        m_header = new ChatHeader(m_chatAreaWidget);
        connect(m_header, &ChatHeader::toggleSidebarRequested, this, [this]() {
            m_splitView->togglePane(0, true);
            m_header->setSidebarExpanded(m_splitView->isPaneExpanded(0));
        });
        chatAreaLayout->addWidget(m_header);

        // 2.2 中部区域（左侧对话锚点 + 中央消息列表 + 右侧占位）
        auto *middleRow = new QWidget(m_chatAreaWidget);
        auto *middleLayout = new QHBoxLayout(middleRow);
        middleLayout->setContentsMargins(4, 6, 8, 6);
        middleLayout->setSpacing(8);

        // 左侧对话锚点条
        m_anchorBar = new ChatAnchorBar(middleRow);
        middleLayout->addWidget(m_anchorBar, 0);

        // 中央主消息列表
        m_messageListView = new MessageListView(middleRow);
        m_messageListView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        middleLayout->addWidget(m_messageListView, 1);

        // 右侧区域（占位）
        m_mainRight = new QWidget(middleRow);
        m_mainRight->setFixedWidth(0);
        middleLayout->addWidget(m_mainRight, 0);

        chatAreaLayout->addWidget(middleRow, 1);

        // 2.3 底部输入框容器
        auto *inputContainer = new QWidget(m_chatAreaWidget);
        auto *inputLayout = new QHBoxLayout(inputContainer);
        inputLayout->setContentsMargins(20, 0, 20, 0);
        inputLayout->setSpacing(0);
        inputLayout->setAlignment(Qt::AlignHCenter);

        m_inputBox = new ChatInputBox(inputContainer);
        inputLayout->addWidget(m_inputBox);

        chatAreaLayout->addWidget(inputContainer);

        // 点击锚点瞬间定位到目标消息
        connect(m_anchorBar, &ChatAnchorBar::anchorClicked, this, [this](int index, const QString &id) {
            m_messageListView->scrollToMessage(id);
            m_viewModel->setActiveAnchorIndex(index);
        });

        // 视口滚动检测：通知 ViewModel 激活对应锚点
        connect(m_messageListView, &MessageListView::topVisibleMessageChanged, m_viewModel, &ChatViewModel::setActiveAnchorByMessageId);

        // 输入框动作：发送消息与停止生成
        connect(m_inputBox, &ChatInputBox::sendRequested, this, [this](const QString &text) {
            m_inputBox->clearText();
            m_viewModel->sendMessage(text);
        });
        connect(m_inputBox, &ChatInputBox::stopRequested, m_viewModel, &ChatViewModel::stopGeneration);

        fluent::collections::SplitViewPaneOptions chatPaneOptions;
        chatPaneOptions.fill = true;
        chatPaneOptions.minimumSize = 300;
        m_splitView->addPane(m_chatAreaWidget, chatPaneOptions);

        // 启动 UDF 状态观察与粘性首次分发
        m_viewModel->observe(this, &ChatPage::render);
    }

    void ChatPage::render(const ChatState &state) {
        // 0. 侧边栏会话列表全量同步（内部 blockSignals 防止重入）
        m_sidebar->setSessions(state.sessions, state.currentSessionId);

        // 1. 顶部会话标题
        m_header->setTitle(state.sessionTitle);

        // 2. 输入控制台状态与模型
        m_inputBox->setModelName(state.currentModelName);
        if (state.isGenerating) {
            m_inputBox->setSendState(ChatInputBox::SendState::Generating);
        } else {
            m_inputBox->setSendState(m_inputBox->text().trimmed().isEmpty()
                ? ChatInputBox::SendState::Idle
                : ChatInputBox::SendState::Ready);
        }

        // 3. 消息列表全量同步 (MessageListView 内部执行增量 Diff)
        m_messageListView->syncMessages(state.messages);

        // 4. 侧边时间线锚点同步
        m_anchorBar->setAnchors(state.anchors);
        m_anchorBar->setActiveIndex(state.activeAnchorIndex);
    }
} // namespace ui::screen::chat
