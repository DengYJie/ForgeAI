#include "WorkPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <FluentQt/TextFields.h>
#include <FluentQt/BasicInput.h>

#include "ui/widget/CollapsibleSplitView.h"
#include "WorkViewModel.h"
#include "ui/widget/chat/ChatInputBox.h"
#include "ui/widget/chat/ChatHeader.h"
#include "ui/widget/message/MessageListView.h"

namespace ui::screen::work {
    WorkPage::WorkPage(
        WorkViewModel *viewModel,
        QWidget *parent
    ) : BasePage(parent),
        m_viewModel(viewModel) {
        setupUi();
        if (m_viewModel) {
            m_viewModel->observe(this, &WorkPage::render);
        }
    }

    void WorkPage::setupUi() {
        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(0, 0, 0, 0);
        m_rootLayout->setSpacing(0);

        m_splitView = new ui::widget::CollapsibleSplitView(this);
        m_rootLayout->addWidget(m_splitView);

        // 1. 左侧项目上下文栏
        m_sidebarWidget = new QWidget(this);
        auto *sidebarLayout = new QVBoxLayout(m_sidebarWidget);
        sidebarLayout->setContentsMargins(16, 24, 16, 24);
        sidebarLayout->setSpacing(12);

        auto *sidebarTitle = new fluent::textfields::Label(tr("项目上下文"), m_sidebarWidget);
        sidebarTitle->setFluentTypography(Typography::FontRole::Subtitle);
        sidebarTitle->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        sidebarLayout->addWidget(sidebarTitle);
        auto *help = new fluent::textfields::Label(tr("AGENTS.md\n项目 Skills\nMCP 配置"), m_sidebarWidget);
        help->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        sidebarLayout->addWidget(help);
        m_projectPathLabel = new fluent::textfields::Label(m_sidebarWidget);
        m_projectPathLabel->setWordWrap(true);
        m_projectPathLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        sidebarLayout->addWidget(m_projectPathLabel);
        auto* chooseProject = new fluent::basicinput::Button(tr("打开项目…"), m_sidebarWidget);
        chooseProject->setFluentStyle(fluent::basicinput::Button::Subtle);
        sidebarLayout->addWidget(chooseProject);
        connect(chooseProject, &QPushButton::clicked, this, [this] {
            const QString startPath = m_viewModel ? m_viewModel->state().projectRoot : QString();
            const QString path = QFileDialog::getExistingDirectory(this, tr("选择项目目录"), startPath);
            if (!path.isEmpty() && m_viewModel) m_viewModel->setProjectRoot(path);
        });

        sidebarLayout->addStretch(1);

        m_splitView->addCollapsiblePane(
            m_sidebarWidget,
            ui::widget::SplitPaneDisplayMode::CompactInline,
            48,
            true,
            260);

        // 2. Right-hand project conversation.  It intentionally follows the
        // same header / message surface / composer hierarchy as ChatPage.
        m_workAreaWidget = new QWidget(this);
        auto *workAreaLayout = new QVBoxLayout(m_workAreaWidget);
        workAreaLayout->setContentsMargins(0, 0, 0, 16);
        workAreaLayout->setSpacing(0);
        m_header = new ui::widget::chat::ChatHeader(m_workAreaWidget);
        m_header->setTitle(tr("新任务"));
        connect(m_header, &ui::widget::chat::ChatHeader::toggleSidebarRequested, this, [this] {
            m_splitView->togglePane(0, true);
            m_header->setSidebarExpanded(m_splitView->isPaneExpanded(0));
        });
        workAreaLayout->addWidget(m_header);

        auto* messageSurface = new QWidget(m_workAreaWidget);
        auto* messageLayout = new QVBoxLayout(messageSurface);
        messageLayout->setContentsMargins(24, 6, 24, 6);
        messageLayout->setSpacing(0);
        m_messageList = new ui::widget::message::MessageListView(messageSurface);
        m_messageList->setHeaderVisible(true);
        m_messageList->setAvatarVisible(true);
        messageLayout->addWidget(m_messageList);
        workAreaLayout->addWidget(messageSurface, 1);

        auto* inputContainer = new QWidget(m_workAreaWidget);
        auto* inputLayout = new QVBoxLayout(inputContainer);
        inputLayout->setContentsMargins(20, 0, 20, 0);
        inputLayout->setSpacing(0);
        m_agentInput = new ui::widget::chat::ChatInputBox(inputContainer);
        m_agentInput->setModelPresentation(tr("项目 Agent"), QString());
        inputLayout->addWidget(m_agentInput, 0, Qt::AlignHCenter);
        workAreaLayout->addWidget(inputContainer);
        if (m_viewModel) {
            connect(m_agentInput, &ui::widget::chat::ChatInputBox::sendRequested, this, [this](const QString& text) { m_viewModel->startTask(text); });
            connect(m_agentInput, &ui::widget::chat::ChatInputBox::stopRequested, m_viewModel, &WorkViewModel::cancelTask);
        }

        fluent::collections::SplitViewPaneOptions workPaneOptions;
        workPaneOptions.fill = true;
        workPaneOptions.minimumSize = 300;
        m_splitView->addPane(m_workAreaWidget, workPaneOptions);
    }

    void WorkPage::render(const WorkState &state) {
        if (!m_messageList) return;
        m_header->setTitle(state.currentTask.isEmpty() ? tr("新任务") : state.currentTask);
        m_projectPathLabel->setText(state.projectRoot.isEmpty() ? tr("未选择项目") : state.projectRoot);
        m_messageList->syncMessages(state.messages);
        m_agentInput->setSendState(state.isProcessing ? ui::widget::chat::ChatInputBox::SendState::Generating : ui::widget::chat::ChatInputBox::SendState::Idle);
    }
} // namespace ui::screen::work
