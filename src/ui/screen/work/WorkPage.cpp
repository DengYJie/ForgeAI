#include "WorkPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QIconEngine>
#include <QStandardItemModel>
#include <FluentQt/TextFields.h>
#include <FluentQt/Collections.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/MenusToolbars.h>

#include "ui/widget/CollapsibleSplitView.h"
#include "WorkViewModel.h"
#include "ProjectSessionTreeDelegate.h"
#include "ui/widget/chat/ChatInputBox.h"
#include "ui/widget/chat/ChatHeader.h"
#include "ui/widget/message/MessageListView.h"

#include "LeftAlignedButton.h"
#include "ProjectHeader.h"
#include "CreateProjectDialog.h"

namespace {
class EditProjectDialog final : public ::fluent::dialogs_flyouts::ContentDialog {
public:
    explicit EditProjectDialog(const QString& currentName, QWidget* parent = nullptr)
        : ContentDialog(parent) {
        setTitle(tr("编辑项目"));
        setPrimaryButtonText(tr("保存"));
        setCloseButtonText(tr("取消"));
        setDefaultButton(Primary);

        auto* content = new QWidget(this);
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 8, 0, 8);
        layout->setSpacing(8);

        auto* label = new fluent::textfields::Label(tr("项目名称"), content);
        label->setFluentTypography(Typography::FontRole::Caption);
        label->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        layout->addWidget(label);

        m_name = new fluent::textfields::LineEdit(content);
        m_name->setText(currentName);
        layout->addWidget(m_name);

        setContent(content);
    }

    QString name() const { return m_name->text().trimmed(); }

private:
    fluent::textfields::LineEdit* m_name = nullptr;
};

class FluentGlyphIconEngine : public QIconEngine, public fluent::FluentElement {
public:
    FluentGlyphIconEngine(const QString& glyph, int pixelSize = 16, const fluent::FluentElement* themeSource = nullptr)
        : m_glyph(glyph), m_pixelSize(pixelSize), m_themeSource(themeSource) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State) override {
        painter->save();
        const auto& colors = m_themeSource ? m_themeSource->themeColorsRef() : themeColorsRef();
        const QColor iconColor = (mode == QIcon::Disabled) ? colors.textDisabled : colors.textPrimary;
        painter->setPen(iconColor);
        Typography::Icons::paintGlyph(*painter, rect, m_glyph, m_pixelSize, Qt::AlignCenter);
        painter->restore();
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pix(size);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        paint(&p, QRect(QPoint(0, 0), size), mode, state);
        return pix;
    }

    QIconEngine* clone() const override {
        return new FluentGlyphIconEngine(m_glyph, m_pixelSize, m_themeSource);
    }

private:
    QString m_glyph;
    int m_pixelSize;
    const fluent::FluentElement* m_themeSource = nullptr;
};

inline QIcon makeFluentIcon(const QString& glyph, int pixelSize = 16, const fluent::FluentElement* themeSource = nullptr) {
    return QIcon(new FluentGlyphIconEngine(glyph, pixelSize, themeSource));
}
} // namespace

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

        auto* treeHost = new QWidget(this);
        auto* treeLayout = new QVBoxLayout(treeHost);
        treeLayout->setContentsMargins(8, 10, 8, 10);
        treeLayout->setSpacing(6);
        m_newConversationButton = new LeftAlignedButton(treeHost);
        m_newConversationButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
        m_newConversationButton->setFluentLayout(::fluent::basicinput::Button::IconBefore);
        m_newConversationButton->setFluentSize(::fluent::basicinput::Button::Small);
        m_newConversationButton->setIconGlyph(Typography::Icons::Edit, 13);
        QFont btnFont = themeFont(Typography::FontRole::Body).toQFont();
        btnFont.setPixelSize(Typography::FontSize::Caption);
        m_newConversationButton->setFont(btnFont);
        m_newConversationButton->setText(tr("新对话"));
        m_newConversationButton->setCursor(Qt::PointingHandCursor);
        m_newConversationButton->setFixedHeight(32);
        m_newConversationButton->setToolTip(tr("新建对话"));
        treeLayout->addWidget(m_newConversationButton);

        m_treeHeader = new ProjectHeader(treeHost);
        treeLayout->addWidget(m_treeHeader);

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
        treeLayout->addWidget(m_sessionTree, 1);
        treeLayout->addStretch(0);

        connect(m_treeHeader, &ProjectHeader::expandToggled, this, [this](bool expanded) {
            m_sessionTree->setHidden(!expanded);
        });

        if (m_viewModel) {
            connect(m_treeHeader, &ProjectHeader::addProjectClicked, this, [this] {
                CreateProjectDialog dialog(this);
                if (dialog.exec() == QDialog::Accepted) m_viewModel->addProject(dialog.path(), dialog.name());
            });
            connect(m_newConversationButton, &QPushButton::clicked, m_viewModel, &WorkViewModel::newSession);
            connect(m_sessionTree, &QTreeView::clicked, this, [this](const QModelIndex& index) {
                if (index.data(Qt::UserRole + 3).toInt() == ProjectSessionTreeDelegate::ShowMoreItem) {
                    m_expandedProjects.insert(index.data(Qt::UserRole + 2).toUuid());
                    if (m_viewModel) render(m_viewModel->state());
                    return;
                }
                const QString sessionId = index.data(Qt::UserRole + 1).toString();
                if (!sessionId.isEmpty()) { m_viewModel->loadSession(sessionId); return; }
                const QUuid projectId = index.data(Qt::UserRole + 2).toUuid();
                if (!projectId.isNull()) {
                    bool willExpand = !m_sessionTree->isExpanded(index);
                    m_sessionTree->setExpanded(index, willExpand);
                    if (willExpand) m_collapsedProjects.remove(projectId);
                    else m_collapsedProjects.insert(projectId);
                    m_viewModel->selectProject(projectId);
                }
            });
            connect(static_cast<ProjectSessionTreeDelegate*>(m_sessionTreeDelegate),
                    &ProjectSessionTreeDelegate::projectMoreRequested, this, &WorkPage::showProjectContextMenu);
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
        m_sessionTreeModel->clear();

        auto sortedProjects = state.projects;
        std::stable_sort(sortedProjects.begin(), sortedProjects.end(), [&](const auto& a, const auto& b) {
            bool aPinned = state.pinnedProjects.contains(a.id);
            bool bPinned = state.pinnedProjects.contains(b.id);
            if (aPinned != bPinned) return aPinned > bPinned;
            return false;
        });

        for (const auto& projectData : sortedProjects) {
            auto* project = new QStandardItem(projectData.name);
            project->setEditable(false);
            project->setFlags(project->flags() & ~Qt::ItemIsSelectable);
            project->setData(projectData.id, Qt::UserRole + 2);
            project->setData(ProjectSessionTreeDelegate::ProjectItem, Qt::UserRole + 3);
            if (state.pinnedProjects.contains(projectData.id)) {
                project->setData(true, Qt::UserRole + 4);
            }
            
            const bool isExpanded = m_expandedProjects.contains(projectData.id);
            QList<ui::screen::chat::ChatSessionItemData> projectSessions;
            for (const auto& session : state.sessions) {
                if (!session.isArchived && session.projectId == projectData.id) {
                    projectSessions.append(session);
                }
            }
            std::stable_sort(projectSessions.begin(), projectSessions.end(), [](const auto& a, const auto& b) {
                if (a.isPinned != b.isPinned) return a.isPinned;
                return a.timestamp > b.timestamp;
            });
            
            const int totalSessions = projectSessions.size();
            int count = 0;
            for (const auto& session : projectSessions) {
                if (!isExpanded && count >= 5 && totalSessions > 5) {
                    auto* showMore = new QStandardItem(tr("展开显示"));
                    showMore->setData(projectData.id, Qt::UserRole + 2);
                    showMore->setData(ProjectSessionTreeDelegate::ShowMoreItem, Qt::UserRole + 3);
                    showMore->setEditable(false);
                    showMore->setFlags(showMore->flags() & ~Qt::ItemIsSelectable);
                    project->appendRow(showMore);
                    break;
                }
                
                auto* leaf = new QStandardItem(session.title);
                leaf->setData(session.id, Qt::UserRole + 1);
                leaf->setData(ProjectSessionTreeDelegate::ConversationItem, Qt::UserRole + 3);
                leaf->setData(session.isPinned, Qt::UserRole + 4);
                const bool isProcessing = (state.isProcessing && session.id == state.currentSessionId);
                leaf->setData(isProcessing, Qt::UserRole + 5);
                leaf->setEditable(false);
                project->appendRow(leaf);
                count++;
            }
            m_sessionTreeModel->appendRow(project);
            if (!m_collapsedProjects.contains(projectData.id)) {
                m_sessionTree->expand(project->index());
            }
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

    void WorkPage::showProjectContextMenu(const QUuid &projectId, const QPoint &globalPos) {
        if (!m_viewModel) return;
        const auto state = m_viewModel->state();
        auto it = std::find_if(state.projects.cbegin(), state.projects.cend(),
                               [&](const auto& p) { return p.id == projectId; });
        if (it == state.projects.cend()) return;
        const auto project = *it;
        const bool isPinned = state.pinnedProjects.contains(projectId);

        auto* menu = new fluent::menus_toolbars::FluentMenu(QString(), this);
        menu->setMinimumWidth(180);

        // 1. 置顶 / 取消置顶
        QAction* pinAction = menu->addAction(
            makeFluentIcon(isPinned ? Typography::Icons::PinFill : Typography::Icons::Pin, 16, menu),
            isPinned ? tr("取消置顶") : tr("置顶")
        );
        connect(pinAction, &QAction::triggered, this, [this, projectId] {
            m_viewModel->toggleProjectPinned(projectId);
        });

        // 2. 编辑
        QAction* editAction = menu->addAction(makeFluentIcon(Typography::Icons::Edit, 16, menu), tr("编辑"));
        connect(editAction, &QAction::triggered, this, [this, project] {
            EditProjectDialog dialog(project.name, this);
            if (dialog.exec() == QDialog::Accepted && !dialog.name().isEmpty()) {
                m_viewModel->renameProject(project.id, dialog.name());
            }
        });

        menu->addSeparator();

        // 3. 在资源管理器中打开
        QAction* openExplorerAction = menu->addAction(makeFluentIcon(Typography::Icons::Folder, 16, menu), tr("在资源管理器中打开"));
        connect(openExplorerAction, &QAction::triggered, this, [project] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(project.rootPath));
        });

        menu->addSeparator();

        // 4. 归档聊天
        QAction* archiveAction = menu->addAction(makeFluentIcon(QString(QChar(0xE7B8)), 16, menu), tr("归档聊天"));
        connect(archiveAction, &QAction::triggered, this, [this, projectId] {
            m_viewModel->archiveProjectSessions(projectId);
        });

        menu->addSeparator();

        // 5. 移除项目
        QAction* removeAction = menu->addAction(makeFluentIcon(Typography::Icons::Dismiss, 16, menu), tr("移除项目"));
        connect(removeAction, &QAction::triggered, this, [this, projectId] {
            m_viewModel->removeProject(projectId);
        });

        menu->exec(globalPos);
        menu->deleteLater();
    }
} // namespace ui::screen::work
