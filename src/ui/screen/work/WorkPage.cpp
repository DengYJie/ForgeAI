#include "WorkPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QIconEngine>
#include <QStandardItemModel>
#include <QScrollArea>
#include <QLineEdit>
#include <QFrame>
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
#include "ui/widget/chat/ConversationPane.h"
#include "ui/widget/message/MessageListView.h"

#include "ui/widget/basic/LeftAlignedButton.h"
#include "ProjectHeader.h"
#include "CreateProjectDialog.h"

namespace {
class ModelChoiceButton final : public fluent::basicinput::Button {
public:
    using fluent::basicinput::Button::Button;
protected:
    QRectF contentPaintRect(const QRectF& surfaceRect) const override {
        constexpr int leftInset = 12;
        const int textWidth = fontMetrics().horizontalAdvance(text());
        return QRectF(surfaceRect.left() + leftInset, surfaceRect.top(), textWidth, surfaceRect.height());
    }
};

class ModelPickerPopup final : public QFrame, public fluent::FluentElement {
public:
    explicit ModelPickerPopup(QWidget* parent = nullptr)
        : QFrame(parent) {
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
    }
    void onThemeUpdated() override { update(); }
protected:
    void paintEvent(QPaintEvent*) override {}
};

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

using widget::basic::LeftAlignedButton;
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

        fluent::collections::SplitViewPaneOptions sidebarOptions;
        sidebarOptions.minimumSize = 200;
        sidebarOptions.maximumSize = 380;
        sidebarOptions.preferredSize = 260;

        m_splitView->addCollapsiblePane(
            treeHost,
            ui::widget::SplitPaneDisplayMode::Inline,
            /*compactLength=*/0,
            /*startExpanded=*/true,
            /*initialOpenLength=*/260,
            sidebarOptions);
        m_splitView->setAutoCollapseBreakpoint(0, 768);

        // 2. Right-hand project conversation.  It intentionally follows the
        // same header / message surface / composer hierarchy as ChatPage.
        m_workAreaWidget = new QWidget(this);
        auto *workAreaLayout = new QVBoxLayout(m_workAreaWidget);
        workAreaLayout->setContentsMargins(0, 0, 0, 0);
        workAreaLayout->setSpacing(0);
        m_pane = new widget::chat::ConversationPane(m_workAreaWidget);
        m_pane->header()->setTitle(tr("新任务"));
        m_pane->setAnchorBarVisible(false);
        m_pane->messageList()->setHeaderVisible(true);
        m_pane->messageList()->setAvatarVisible(true);
        m_pane->inputBox()->setModelPresentation(tr("选择模型"), QString());
        workAreaLayout->addWidget(m_pane);

        connect(m_pane->header(), &ui::widget::chat::ChatHeader::toggleSidebarRequested, this, [this] {
            m_splitView->togglePane(0, true);
            m_pane->header()->setSidebarExpanded(m_splitView->isPaneExpanded(0));
        });
        connect(m_splitView, &ui::widget::CollapsibleSplitView::paneOpened, this, [this](int index) {
            if (index == 0) m_pane->header()->setSidebarExpanded(true);
        });
        connect(m_splitView, &ui::widget::CollapsibleSplitView::paneClosed, this, [this](int index) {
            if (index == 0) m_pane->header()->setSidebarExpanded(false);
        });
        connect(m_pane->inputBox(), &ui::widget::chat::ChatInputBox::modelButtonClicked, this, &WorkPage::showModelPicker);
        if (m_viewModel) {
            connect(m_pane->inputBox(), &ui::widget::chat::ChatInputBox::sendRequested, this, [this](const QString& text) { m_viewModel->startTask(text); });
            connect(m_pane->inputBox(), &ui::widget::chat::ChatInputBox::stopRequested, m_viewModel, &WorkViewModel::cancelTask);
            connect(m_pane->inputBox(), &ui::widget::chat::ChatInputBox::webSearchToggled, m_viewModel, &WorkViewModel::setWebSearchEnabled);
            connect(m_pane->inputBox(), &ui::widget::chat::ChatInputBox::deepThinkToggled, m_viewModel, &WorkViewModel::setDeepThinkingEnabled);
        }

        fluent::collections::SplitViewPaneOptions workPaneOptions;
        workPaneOptions.fill = true;
        workPaneOptions.minimumSize = 300;
        m_splitView->addPane(m_workAreaWidget, workPaneOptions);
    }

    void WorkPage::render(const WorkState &state) {
        if (!m_pane->messageList()) return;
        m_pane->header()->setTitle(state.currentSessionId.isEmpty()
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
        m_pane->messageList()->syncMessages(state.messages);
        m_pane->statusLabel()->setText(state.statusMessage);
        m_pane->statusLabel()->setVisible(!state.statusMessage.isEmpty());
        m_pane->inputBox()->setEnabled(!state.currentSessionId.isEmpty());
        m_pane->inputBox()->setSendState(state.isProcessing ? ui::widget::chat::ChatInputBox::SendState::Generating : ui::widget::chat::ChatInputBox::SendState::Idle);
        m_pane->inputBox()->setModelPresentation(state.currentModelName, state.reasoningEffort);
        const auto current = std::find_if(state.availableModels.cbegin(), state.availableModels.cend(), [&](const ui::screen::chat::ChatModelOption& option) {
            return option.providerId == state.currentModelProviderId && option.modelId == state.currentModelId;
        });
        const bool hasReasoningEffort = (current != state.availableModels.cend()) && !current->reasoningEfforts.isEmpty();
        m_pane->inputBox()->setToolAvailability(true, true, hasReasoningEffort);
        updateModelChoices(state);
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

    void WorkPage::updateModelChoices(const WorkState& state) {
        m_modelChoices.clear();
        m_currentModelProviderId = state.currentModelProviderId;
        m_currentModelId = state.currentModelId;
        m_modelChoices.reserve(state.availableModels.size());
        for (const auto& option : state.availableModels) {
            m_modelChoices.push_back({option.providerId, option.modelId, option.displayName, option.providerName});
        }
        if (m_modelPickerPopup && m_modelPickerPopup->isVisible()) rebuildModelPicker({});
    }

    void WorkPage::showModelPicker() {
        if (!m_pane->inputBox() || !m_pane->inputBox()->modelAnchor()) return;
        m_pane->inputBox()->notifyModelMenuOpened();

        auto* menu = new fluent::menus_toolbars::FluentMenu(QString(), this);
        menu->setMinimumWidth(220);
        connect(menu, &fluent::menus_toolbars::FluentMenu::aboutToHide, this, [this]() {
            if (m_pane && m_pane->inputBox()) {
                m_pane->inputBox()->notifyModelMenuClosed();
            }
        });

        auto* modelMenu = new fluent::menus_toolbars::FluentMenu(tr("模型"), menu);
        modelMenu->setMinimumWidth(260);
        modelMenu->menuAction()->setText(tr("模型\t%1").arg(m_pane->inputBox()->modelName()));
        QString previousProvider;
        for (const auto& choice : m_modelChoices) {
            if (choice.providerName != previousProvider) {
                modelMenu->addSection(choice.providerName.isEmpty() ? tr("模型") : choice.providerName);
                previousProvider = choice.providerName;
            }
            QAction* action = modelMenu->addAction(choice.displayName);
            action->setCheckable(true);
            action->setChecked(choice.providerId == m_currentModelProviderId && choice.modelId == m_currentModelId);
            connect(action, &QAction::triggered, this, [this, choice] {
                if (m_viewModel) m_viewModel->setModel(choice.providerId, choice.modelId);
            });
        }
        if (modelMenu->actions().isEmpty()) {
            QAction* empty = modelMenu->addAction(tr("没有可用模型"));
            empty->setEnabled(false);
        }
        menu->addMenu(modelMenu);
        const auto current = std::find_if(m_modelChoices.cbegin(), m_modelChoices.cend(), [this](const ModelChoice& choice) {
            return choice.providerId == m_currentModelProviderId && choice.modelId == m_currentModelId;
        });
        if (current != m_modelChoices.cend()) {
            auto* effortMenu = new fluent::menus_toolbars::FluentMenu(tr("推理强度"), menu);
            const auto state = m_viewModel ? m_viewModel->state() : WorkState{};
            const auto option = std::find_if(state.availableModels.cbegin(), state.availableModels.cend(), [this](const ui::screen::chat::ChatModelOption& item) {
                return item.providerId == m_currentModelProviderId && item.modelId == m_currentModelId;
            });
            if (option != state.availableModels.cend()) {
                for (const auto& effort : option->reasoningEfforts) {
                    QAction* action = effortMenu->addAction(effort);
                    action->setCheckable(true); action->setChecked(effort == state.reasoningEffort);
                    connect(action, &QAction::triggered, this, [this, effort] { if (m_viewModel) m_viewModel->setReasoningEffort(effort); });
                }
            }
            if (!effortMenu->actions().isEmpty()) menu->addMenu(effortMenu);
        }
        menu->popup(m_pane->inputBox()->modelAnchor()->mapToGlobal(QPoint(0, -menu->sizeHint().height())));
    }

    void WorkPage::showFullModelPicker(const QPoint& globalOrigin) {
        if (!m_pane->inputBox() || !m_pane->inputBox()->modelAnchor()) return;
        if (m_modelPickerPopup) m_modelPickerPopup->close();
        auto* popup = new ModelPickerPopup(nullptr);
        m_modelPickerPopup = popup;
        popup->setFixedWidth(280);
        auto* layout = new QVBoxLayout(popup);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);
        auto* search = new fluent::textfields::LineEdit(popup);
        search->setPlaceholderText(tr("搜索模型…"));
        search->setClearButtonEnabled(true);
        search->setFixedHeight(32);
        layout->addWidget(search);
        auto* scroll = new QScrollArea(popup);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; } QScrollArea > QWidget > QWidget { background: transparent; }"));
        scroll->setWidgetResizable(true);
        auto* rows = new QWidget(scroll);
        rows->setObjectName(QStringLiteral("modelPickerRows"));
        scroll->setWidget(rows);
        m_modelPickerRows = rows;
        layout->addWidget(scroll, 1);
        connect(search, &QLineEdit::textChanged, this, &WorkPage::rebuildModelPicker);
        connect(popup, &QObject::destroyed, this, [this, popup] {
            if (m_modelPickerPopup == popup) { m_modelPickerPopup = nullptr; m_modelPickerRows = nullptr; m_modelPickerOrigin = {}; }
        });
        rebuildModelPicker({});
        const QPoint global = globalOrigin.isNull()
            ? m_pane->inputBox()->modelAnchor()->mapToGlobal(QPoint(m_pane->inputBox()->modelAnchor()->width() - popup->width(), 0))
            : globalOrigin;
        m_modelPickerOrigin = globalOrigin;
        popup->move(globalOrigin.isNull() ? global - QPoint(0, popup->height() + 8) : global);
        popup->show();
        search->setFocus();
    }

    void WorkPage::rebuildModelPicker(const QString& query) {
        if (!m_modelPickerRows) return;
        auto* old = m_modelPickerRows->layout();
        if (old) {
            while (QLayoutItem* item = old->takeAt(0)) { delete item->widget(); delete item; }
            delete old;
        }
        auto* layout = new QVBoxLayout(m_modelPickerRows);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        const QString filter = query.trimmed();
        QMap<QString, QVector<ModelChoice>> groups;
        for (const auto& choice : m_modelChoices) {
            const QString searchable = choice.displayName + QLatin1Char(' ') + choice.providerName;
            if (filter.isEmpty() || searchable.contains(filter, Qt::CaseInsensitive)) groups[choice.providerName].push_back(choice);
        }
        bool hasMatch = false;
        int visibleRows = 0;
        for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
            auto* header = new fluent::textfields::Label(it.key().isEmpty() ? tr("模型") : it.key(), m_modelPickerRows);
            header->setFluentTypography(Typography::FontRole::Caption);
            header->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
            header->setContentsMargins(12, visibleRows == 0 ? 4 : 10, 8, 3);
            layout->addWidget(header);
            ++visibleRows;
            for (const auto& choice : it.value()) {
                auto* button = new ModelChoiceButton(m_modelPickerRows);
                button->setFluentLayout(fluent::basicinput::Button::TextOnly);
                button->setFluentStyle(fluent::basicinput::Button::Subtle);
                button->setText(choice.displayName);
                button->setFixedHeight(34);
                const bool selected = choice.providerId == m_currentModelProviderId && choice.modelId == m_currentModelId;
                button->setCheckable(true);
                button->setChecked(selected);
                button->setCursor(Qt::PointingHandCursor);
                connect(button, &QPushButton::clicked, this, [this, choice] {
                    if (m_viewModel) m_viewModel->setModel(choice.providerId, choice.modelId);
                    if (m_modelPickerPopup) m_modelPickerPopup->close();
                });
                layout->addWidget(button);
                hasMatch = true;
                ++visibleRows;
            }
        }
        if (!hasMatch) {
            auto* empty = new fluent::textfields::Label(tr("没有匹配的模型"), m_modelPickerRows);
            empty->setAlignment(Qt::AlignCenter);
            empty->setContentsMargins(0, 24, 0, 24);
            layout->addWidget(empty);
        }
        layout->addStretch();
        if (m_modelPickerPopup) {
            const int rowsHeight = hasMatch ? visibleRows * 34 + groups.size() * 6 : 88;
            m_modelPickerPopup->setFixedHeight(qBound(120, rowsHeight + 54, 280));
            if (m_modelPickerPopup->isVisible() && !m_modelPickerOrigin.isNull()) {
                m_modelPickerPopup->move(m_modelPickerOrigin);
            } else if (m_modelPickerPopup->isVisible() && m_pane->inputBox() && m_pane->inputBox()->modelAnchor()) {
                const QPoint global = m_pane->inputBox()->modelAnchor()->mapToGlobal(QPoint(m_pane->inputBox()->modelAnchor()->width() - m_modelPickerPopup->width(), 0));
                m_modelPickerPopup->move(global - QPoint(0, m_modelPickerPopup->height() + 8));
            }
        }
    }
} // namespace ui::screen::work
