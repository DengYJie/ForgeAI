#include "ChatPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QLabel>
#include <QStackedLayout>
#include <QMap>
#include <QFrame>
#include <QPainter>

#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/MenusToolbars.h>

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
    }

    ChatPage::ChatPage(
        ChatViewModel *viewModel,
        QWidget *parent
    ) : BasePage(parent),
        m_viewModel(viewModel) {
        setupUi();
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

        if (m_viewModel) {
            connect(m_sidebar, &ChatSidebar::newChatRequested, m_viewModel, &ChatViewModel::newSession);
            connect(m_sidebar, &ChatSidebar::sessionSelected, m_viewModel, &ChatViewModel::loadSession);
            connect(m_sidebar, &ChatSidebar::sessionDeleted, m_viewModel, &ChatViewModel::deleteSession);
            connect(m_sidebar, &ChatSidebar::sessionPinToggled, m_viewModel, &ChatViewModel::setSessionPinned);
            connect(m_sidebar, &ChatSidebar::sessionArchiveRequested, m_viewModel,
                    [this](const QString& id) { m_viewModel->setSessionArchived(id, true); });
        }

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
        if (m_viewModel) {
            connect(m_header, &ChatHeader::clearChatRequested, m_viewModel, &ChatViewModel::clearCurrentSession);
        }
        chatAreaLayout->addWidget(m_header);

        // 2.2 中部区域（左侧对话锚点 + 中央消息列表 + 右侧占位）
        auto *middleRow = new QWidget(m_chatAreaWidget);
        auto *middleLayout = new QHBoxLayout(middleRow);
        middleLayout->setContentsMargins(4, 6, 8, 6);
        middleLayout->setSpacing(8);

        // 左侧对话锚点条
        m_anchorBar = new ChatAnchorBar(middleRow);
        middleLayout->addWidget(m_anchorBar, 0);

        // 中央消息表面：列表与轻量空态叠放，保持输入区始终可用。
        auto* messageSurface = new QWidget(middleRow);
        auto* messageStack = new QStackedLayout(messageSurface);
        messageStack->setContentsMargins(0, 0, 0, 0);
        messageStack->setStackingMode(QStackedLayout::StackAll);
        m_messageListView = new MessageListView(messageSurface);
        m_messageListView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        messageStack->addWidget(m_messageListView);
        m_emptyStateLabel = new fluent::textfields::Label(tr("开始新对话\n\n选择模型，然后输入你的问题。"), messageSurface);
        m_emptyStateLabel->setFluentTypography(Typography::FontRole::Body);
        m_emptyStateLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_emptyStateLabel->setAlignment(Qt::AlignCenter);
        m_emptyStateLabel->setWordWrap(true);
        m_emptyStateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        messageStack->addWidget(m_emptyStateLabel);
        middleLayout->addWidget(messageSurface, 1);

        chatAreaLayout->addWidget(middleRow, 1);

        // 2.3 底部输入框容器
        auto *inputContainer = new QWidget(m_chatAreaWidget);
        auto *inputLayout = new QVBoxLayout(inputContainer);
        inputLayout->setContentsMargins(20, 0, 20, 0);
        inputLayout->setSpacing(4);

        m_inputBox = new ChatInputBox(inputContainer);
        inputLayout->addWidget(m_inputBox, 0, Qt::AlignHCenter);

        m_statusLabel = new fluent::textfields::Label(inputContainer);
        m_statusLabel->setFluentTypography(Typography::FontRole::Caption);
        m_statusLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_statusLabel->setAlignment(Qt::AlignHCenter);
        m_statusLabel->hide();
        inputLayout->addWidget(m_statusLabel);

        chatAreaLayout->addWidget(inputContainer);

        // 点击锚点瞬间定位到目标消息
        if (m_viewModel) {
            connect(m_anchorBar, &ChatAnchorBar::anchorClicked, this, [this](int index, const QString &id) {
                m_messageListView->scrollToMessage(id);
                m_viewModel->setActiveAnchorIndex(index);
            });

            // 视口滚动检测：通知 ViewModel 激活对应锚点
            connect(m_messageListView, &MessageListView::topVisibleMessageChanged, m_viewModel, &ChatViewModel::setActiveAnchorByMessageId);

            // 输入框动作：发送消息与停止生成
            connect(m_inputBox, &ChatInputBox::sendRequested, this, [this](const QString &text) {
                m_viewModel->sendMessage(text);
            });
            connect(m_inputBox, &ChatInputBox::stopRequested, m_viewModel, &ChatViewModel::stopGeneration);
            connect(m_inputBox, &ChatInputBox::modelButtonClicked, this, &ChatPage::showModelPicker);
            connect(m_inputBox, &ChatInputBox::attachClicked, this, [this] {
                m_statusLabel->setText(tr("当前模型协议仅支持文本消息，附件功能尚不可用。"));
                m_statusLabel->show();
            });
            connect(m_inputBox, &ChatInputBox::webSearchToggled, m_viewModel, &ChatViewModel::setWebSearchEnabled);
            connect(m_inputBox, &ChatInputBox::deepThinkToggled, m_viewModel, &ChatViewModel::setDeepThinkingEnabled);
        }

        fluent::collections::SplitViewPaneOptions chatPaneOptions;
        chatPaneOptions.fill = true;
        chatPaneOptions.minimumSize = 300;
        m_splitView->addPane(m_chatAreaWidget, chatPaneOptions);

        // 启动 UDF 状态观察与粘性首次分发
        if (m_viewModel) {
            m_viewModel->observe(this, &ChatPage::render);
        }
    }

    void ChatPage::render(const ChatState &state) {
        // 0. 侧边栏会话列表全量同步（内部 blockSignals 防止重入）
        QList<ChatSessionItemData> visibleSessions;
        for (const auto& session : state.sessions) {
            if (!session.isArchived) visibleSessions.append(session);
        }
        m_sidebar->setSessions(visibleSessions, state.currentSessionId);

        // 1. 顶部会话标题
        m_header->setTitle(state.sessionTitle);

        // 2. 输入控制台状态与模型
        updateModelChoices(state);
        m_inputBox->setModelPresentation(state.currentModelName, state.useDeepThinking ? state.reasoningEffort : QString());
        const auto selected = std::find_if(state.availableModels.cbegin(), state.availableModels.cend(), [&state](const ChatModelOption& model) {
            return model.providerId == state.currentModelProviderId && model.modelId == state.currentModelId;
        });
        m_inputBox->setToolAvailability(selected != state.availableModels.cend() && selected->supportsAttachments,
                                        selected != state.availableModels.cend() && selected->supportsWebSearch,
                                        selected != state.availableModels.cend() && selected->supportsDeepThinking);
        if (state.isGenerating) {
            m_inputBox->setSendState(ChatInputBox::SendState::Generating);
        } else {
            m_inputBox->setSendState(m_inputBox->text().trimmed().isEmpty()
                ? ChatInputBox::SendState::Idle
                : ChatInputBox::SendState::Ready);
        }
        m_statusLabel->setText(state.statusMessage);
        m_statusLabel->setVisible(!state.statusMessage.isEmpty());

        // 3. 消息列表全量同步 (MessageListView 内部执行增量 Diff)
        m_messageListView->syncMessages(state.messages);
        m_emptyStateLabel->setVisible(state.messages.isEmpty());

        // 4. 侧边时间线锚点同步
        m_anchorBar->setAnchors(state.anchors);
        m_anchorBar->setActiveIndex(state.activeAnchorIndex);
    }

    void ChatPage::updateModelChoices(const ChatState& state) {
        m_modelChoices.clear();
        m_currentModelProviderId = state.currentModelProviderId;
        m_currentModelId = state.currentModelId;
        m_modelChoices.reserve(state.availableModels.size());
        for (const auto& option : state.availableModels) {
            m_modelChoices.push_back({option.providerId, option.modelId, option.displayName, option.providerName});
        }
        if (m_modelPickerPopup && m_modelPickerPopup->isVisible()) rebuildModelPicker({});
    }

    void ChatPage::showModelPicker() {
        if (!m_inputBox || !m_inputBox->modelAnchor()) return;
        auto* menu = new fluent::menus_toolbars::FluentMenu(QString(), this);
        menu->setMinimumWidth(220);
        auto* modelMenu = new fluent::menus_toolbars::FluentMenu(tr("模型"), menu);
        modelMenu->setMinimumWidth(260);
        modelMenu->menuAction()->setText(tr("模型\t%1").arg(m_inputBox->modelName()));
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
                m_viewModel->setModel(choice.providerId, choice.modelId);
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
            const auto state = m_viewModel ? m_viewModel->state() : ChatState{};
            const auto option = std::find_if(state.availableModels.cbegin(), state.availableModels.cend(), [this](const ChatModelOption& item) {
                return item.providerId == m_currentModelProviderId && item.modelId == m_currentModelId;
            });
            if (option != state.availableModels.cend()) {
                for (const auto& effort : option->reasoningEfforts) {
                    QAction* action = effortMenu->addAction(effort);
                    action->setCheckable(true); action->setChecked(effort == state.reasoningEffort);
                    connect(action, &QAction::triggered, this, [this, effort] { m_viewModel->setReasoningEffort(effort); });
                }
            }
            if (!effortMenu->actions().isEmpty()) menu->addMenu(effortMenu);
        }
        menu->popup(m_inputBox->modelAnchor()->mapToGlobal(QPoint(0, -menu->sizeHint().height())));
    }

    void ChatPage::showFullModelPicker(const QPoint& globalOrigin) {
        if (!m_inputBox || !m_inputBox->modelAnchor()) return;
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
        connect(search, &QLineEdit::textChanged, this, &ChatPage::rebuildModelPicker);
        connect(popup, &QObject::destroyed, this, [this, popup] {
            if (m_modelPickerPopup == popup) { m_modelPickerPopup = nullptr; m_modelPickerRows = nullptr; m_modelPickerOrigin = {}; }
        });
        rebuildModelPicker({});
        const QPoint global = globalOrigin.isNull()
            ? m_inputBox->modelAnchor()->mapToGlobal(QPoint(m_inputBox->modelAnchor()->width() - popup->width(), 0))
            : globalOrigin;
        m_modelPickerOrigin = globalOrigin;
        popup->move(globalOrigin.isNull() ? global - QPoint(0, popup->height() + 8) : global);
        popup->show();
        search->setFocus();
    }

    void ChatPage::rebuildModelPicker(const QString& query) {
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
            } else if (m_modelPickerPopup->isVisible() && m_inputBox && m_inputBox->modelAnchor()) {
                const QPoint global = m_inputBox->modelAnchor()->mapToGlobal(QPoint(m_inputBox->modelAnchor()->width() - m_modelPickerPopup->width(), 0));
                m_modelPickerPopup->move(global - QPoint(0, m_modelPickerPopup->height() + 8));
            }
        }
    }
} // namespace ui::screen::chat
