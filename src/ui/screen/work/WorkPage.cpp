#include "WorkPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardItemModel>
#include <FluentQt/TextFields.h>

#include "ui/widget/CollapsibleSplitView.h"
#include "WorkViewModel.h"
#include "ProjectSessionTreeDelegate.h"
#include "ui/widget/chat/ChatInputBox.h"
#include "ui/widget/chat/ChatHeader.h"
#include "ui/widget/message/MessageListView.h"
#include <FluentQt/Collections.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/DialogsFlyouts.h>

namespace ui::screen::work {
namespace {
class CreateProjectDialog final : public fluent::dialogs_flyouts::ContentDialog {
public:
    explicit CreateProjectDialog(QWidget* parent) : ContentDialog(parent) {
        setTitle(tr("创建项目"));
        setPrimaryButtonText(tr("添加项目"));
        setCloseButtonText(tr("取消"));
        setDefaultButton(Primary);

        auto* content = new QWidget(this);
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 8, 0, 8);
        layout->setSpacing(12);
        auto* description = new fluent::textfields::Label(
            tr("添加一个本地文件夹作为项目工作区。项目 Agent 只能读取和修改该文件夹中的文件。"), content);
        description->setWordWrap(true);
        description->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        layout->addWidget(description);
        auto addLabel = [content, layout](const QString& text) {
            auto* label = new fluent::textfields::Label(text, content);
            label->setFluentTypography(Typography::FontRole::Caption);
            label->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
            layout->addWidget(label);
        };
        addLabel(tr("项目名称（可选）"));
        m_name = new fluent::textfields::LineEdit(content);
        m_name->setPlaceholderText(tr("默认使用文件夹名称"));
        layout->addWidget(m_name);
        addLabel(tr("工作区文件夹"));
        m_path = new fluent::textfields::LineEdit(content);
        m_path->setReadOnly(true);
        m_path->setPlaceholderText(tr("选择 Codex 可以读取和编辑的文件夹"));
        layout->addWidget(m_path);
        auto* choose = new fluent::basicinput::Button(tr("浏览…"), content);
        choose->setFluentStyle(fluent::basicinput::Button::Subtle);
        layout->addWidget(choose, 0, Qt::AlignLeft);
        setContent(content);
        connect(choose, &QPushButton::clicked, this, [this] {
            const QString path = QFileDialog::getExistingDirectory(this, tr("选择项目文件夹"));
            if (path.isEmpty()) return;
            m_path->setText(path);
            if (m_name->text().isEmpty()) m_name->setText(QFileInfo(path).fileName());
        });
    }
    QString name() const { return m_name->text(); } QString path() const { return m_path->text(); }
protected:
    void accept() override {
        if (!m_path->text().trimmed().isEmpty()) ContentDialog::accept();
    }
private:
    fluent::textfields::LineEdit* m_name = nullptr;
    fluent::textfields::LineEdit* m_path = nullptr;
};
}
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

        auto* treeHost = new QWidget(this);
        auto* treeLayout = new QVBoxLayout(treeHost);
        treeLayout->setContentsMargins(8, 10, 8, 10);
        treeLayout->setSpacing(6);
        auto* newConversationRow = new QWidget(treeHost);
        auto* newConversationLayout = new QHBoxLayout(newConversationRow);
        newConversationLayout->setContentsMargins(8, 0, 4, 0);
        newConversationLayout->setSpacing(6);
        m_newConversationButton = new fluent::basicinput::Button(newConversationRow);
        m_newConversationButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_newConversationButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_newConversationButton->setIconGlyph(Typography::Icons::Edit, 14);
        m_newConversationButton->setFixedSize(30, 30);
        m_newConversationButton->setToolTip(tr("新建对话"));
        newConversationLayout->addWidget(m_newConversationButton);
        auto* newConversationLabel = new fluent::textfields::Label(tr("新对话"), newConversationRow);
        newConversationLayout->addWidget(newConversationLabel);
        newConversationLayout->addStretch(1);
        m_newConversationAddButton = new fluent::basicinput::Button(newConversationRow);
        m_newConversationAddButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_newConversationAddButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_newConversationAddButton->setIconGlyph(Typography::Icons::Add, 14);
        m_newConversationAddButton->setFixedSize(30, 30);
        m_newConversationAddButton->setToolTip(tr("新建对话"));
        newConversationLayout->addWidget(m_newConversationAddButton);
        treeLayout->addWidget(newConversationRow);

        auto* treeHeader = new QWidget(treeHost);
        auto* treeHeaderLayout = new QHBoxLayout(treeHeader);
        treeHeaderLayout->setContentsMargins(8, 0, 4, 0);
        treeHeaderLayout->setSpacing(4);
        auto* projectLabel = new fluent::textfields::Label(tr("项目"), treeHeader);
        projectLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        treeHeaderLayout->addWidget(projectLabel);
        treeHeaderLayout->addStretch(1);
        auto* moreProjects = new fluent::basicinput::Button(treeHeader);
        moreProjects->setFluentStyle(fluent::basicinput::Button::Subtle);
        moreProjects->setFluentLayout(fluent::basicinput::Button::IconOnly);
        moreProjects->setIconGlyph(Typography::Icons::More, 14);
        moreProjects->setFixedSize(30, 30);
        moreProjects->setToolTip(tr("项目操作"));
        moreProjects->setEnabled(false);
        treeHeaderLayout->addWidget(moreProjects);
        m_addProjectButton = new fluent::basicinput::Button(treeHeader);
        m_addProjectButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_addProjectButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_addProjectButton->setIconGlyph(Typography::Icons::Add, 14);
        m_addProjectButton->setFixedSize(30, 30);
        m_addProjectButton->setToolTip(tr("添加项目"));
        treeHeaderLayout->addWidget(m_addProjectButton);
        treeLayout->addWidget(treeHeader);
        m_sessionTree = new fluent::collections::TreeView(treeHost);
        m_sessionTree->setBackgroundVisible(false);
        m_sessionTree->setBorderVisible(false);
        m_sessionTree->setHeaderHidden(true);
        m_sessionTree->setRootIsDecorated(false);
        m_sessionTree->setItemsExpandable(true);
        m_sessionTree->setIndentation(0);
        m_sessionTree->setUniformRowHeights(false);
        m_sessionTree->setSelectionIndicatorVisible(false);
        m_sessionTree->setMouseTracking(true);
        m_sessionTree->viewport()->setMouseTracking(true);
        m_sessionTree->viewport()->setAutoFillBackground(false);
        m_sessionTree->setProperty("fluentPreserveParentSurface", true);
        m_sessionTree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_sessionTreeModel = new QStandardItemModel(m_sessionTree);
        m_sessionTree->setModel(m_sessionTreeModel);
        m_sessionTreeDelegate = new ProjectSessionTreeDelegate(m_sessionTree);
        m_sessionTree->setItemDelegate(m_sessionTreeDelegate);
        treeLayout->addWidget(m_sessionTree);
        if (m_viewModel) {
            connect(m_addProjectButton, &QPushButton::clicked, this, [this] {
                CreateProjectDialog dialog(this);
                if (dialog.exec() == QDialog::Accepted) m_viewModel->addProject(dialog.path(), dialog.name());
            });
            connect(m_newConversationButton, &QPushButton::clicked, m_viewModel, &WorkViewModel::newSession);
            connect(m_newConversationAddButton, &QPushButton::clicked, m_viewModel, &WorkViewModel::newSession);
            connect(m_sessionTree, &QTreeView::clicked, this, [this](const QModelIndex& index) {
                const QString sessionId = index.data(Qt::UserRole + 1).toString();
                if (!sessionId.isEmpty()) { m_viewModel->loadSession(sessionId); return; }
                const QUuid projectId = index.data(Qt::UserRole + 2).toUuid();
                if (!projectId.isNull()) m_viewModel->selectProject(projectId);
            });
            connect(static_cast<ProjectSessionTreeDelegate*>(m_sessionTreeDelegate),
                    &ProjectSessionTreeDelegate::newConversationRequested, this, [this](const QUuid& projectId) {
                        m_viewModel->selectProject(projectId);
                        m_viewModel->newSession();
                    });
            connect(static_cast<ProjectSessionTreeDelegate*>(m_sessionTreeDelegate), &ProjectSessionTreeDelegate::pinClicked,
                    m_viewModel, &WorkViewModel::setSessionPinned);
            connect(static_cast<ProjectSessionTreeDelegate*>(m_sessionTreeDelegate), &ProjectSessionTreeDelegate::archiveClicked,
                    this, [this](const QString& id) { m_viewModel->setSessionArchived(id, true); });
        }

        m_splitView->addCollapsiblePane(
            treeHost,
            ui::widget::SplitPaneDisplayMode::Inline,
            0,
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
        m_header->setTitle(state.currentSessionId.isEmpty()
            ? tr("选择项目以新建对话")
            : (state.currentTask.isEmpty() ? tr("新对话") : state.currentTask));
        const bool canCreateConversation = !state.currentProjectId.isNull();
        m_newConversationButton->setEnabled(canCreateConversation);
        m_newConversationAddButton->setEnabled(canCreateConversation);
        m_sessionTreeModel->clear();
        for (const auto& projectData : state.projects) {
            auto* project = new QStandardItem(projectData.name);
            project->setEditable(false);
            project->setData(projectData.id, Qt::UserRole + 2);
            project->setData(ProjectSessionTreeDelegate::ProjectItem, Qt::UserRole + 3);
            for (const auto& session : state.sessions) {
                if (session.isArchived || session.projectId != projectData.id) continue;
                auto* leaf = new QStandardItem(session.title);
                leaf->setData(session.id, Qt::UserRole + 1);
                leaf->setData(ProjectSessionTreeDelegate::ConversationItem, Qt::UserRole + 3);
                leaf->setData(session.isPinned, Qt::UserRole + 4);
                leaf->setEditable(false);
                project->appendRow(leaf);
            }
            m_sessionTreeModel->appendRow(project);
            m_sessionTree->expand(project->index());
        }
        if (!state.currentSessionId.isEmpty()) {
            const auto matches = m_sessionTreeModel->match(
                m_sessionTreeModel->index(0, 0), Qt::UserRole + 1, state.currentSessionId, 1,
                Qt::MatchExactly | Qt::MatchRecursive);
            if (!matches.isEmpty()) m_sessionTree->setCurrentIndex(matches.first());
        } else if (!state.currentProjectId.isNull()) {
            const auto matches = m_sessionTreeModel->match(
                m_sessionTreeModel->index(0, 0), Qt::UserRole + 2, state.currentProjectId, 1,
                Qt::MatchExactly | Qt::MatchRecursive);
            if (!matches.isEmpty()) m_sessionTree->setCurrentIndex(matches.first());
        }
        m_messageList->syncMessages(state.messages);
        m_agentInput->setEnabled(!state.currentSessionId.isEmpty());
        m_agentInput->setSendState(state.isProcessing ? ui::widget::chat::ChatInputBox::SendState::Generating : ui::widget::chat::ChatInputBox::SendState::Idle);
    }
} // namespace ui::screen::work
