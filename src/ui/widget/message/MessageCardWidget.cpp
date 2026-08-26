#include "blocks/AbstractToolBlockWidget.h"
#include "blocks/ErrorBlockWidget.h"
#include "blocks/ImageBlockWidget.h"
#include "blocks/ThinkingBlockWidget.h"
#include "blocks/ToolWidgetFactory.h"
#include "MessageCardWidget.h"
#include "ProcessGroupWidget.h"
#include "ui/widget/MarkdownView.h"

#include <FluentQt/BasicInput.h>
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>
#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <FluentQt/Design.h>

namespace ui::widget::message {

    namespace {

        QString formatTime(const QDateTime& dt)
        {
            if (!dt.isValid())
                return {};
            return dt.toString(QStringLiteral("HH:mm:ss"));
        }

    } // namespace

    MessageCardWidget::MessageCardWidget(QWidget* parent)
        : QWidget(parent)
    {
        setupUi();
    }

    MessageCardWidget::MessageCardWidget(const domain::conversation::Message& message, QWidget* parent)
        : QWidget(parent)
        , m_message(message)
    {
        setupUi();
        syncMessage(message);
    }

    MessageCardWidget::~MessageCardWidget() = default;

    void MessageCardWidget::setupUi()
    {
        QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);

        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->setContentsMargins(12, 4, 12, 4);
        m_mainLayout->setSpacing(2);

        m_headerWidget = new QWidget(this);
        m_headerLayout = new QHBoxLayout(m_headerWidget);
        m_headerLayout->setContentsMargins(0, 0, 0, 0);
        m_headerLayout->setSpacing(8);

        m_avatar = new fluent::status_info::Avatar(this);
        m_avatar->setAvatarSize(fluent::status_info::Avatar::AvatarSize::Medium);
        m_avatar->setShape(fluent::status_info::Avatar::AvatarShape::Circular);

        m_senderLabel = new fluent::textfields::Label(this);
        m_senderLabel->setFluentTypography(Typography::FontRole::BodyStrong);
        m_senderLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

        m_timeLabel = new fluent::textfields::Label(this);
        m_timeLabel->setFluentTypography(Typography::FontRole::Caption);
        m_timeLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

        m_mainLayout->addWidget(m_headerWidget);

        m_contentRow = new QWidget(this);
        m_contentRowLayout = new QHBoxLayout(m_contentRow);
        m_contentRowLayout->setContentsMargins(0, 0, 0, 0);
        m_contentRowLayout->setSpacing(8);

        m_userBubbleCard = new fluent::layout::Card(m_contentRow);
        m_userBubbleCard->setAppearance(fluent::layout::Card::LayerAlt);
        m_userBubbleCard->setBorderVisible(true);

        m_bubbleLayout = new QVBoxLayout(m_userBubbleCard);
        m_bubbleLayout->setContentsMargins(10, 6, 10, 6);
        m_bubbleLayout->setSpacing(2);

        m_markdownView = new ui::widget::MarkdownView(this);
        m_markdownView->setTransparentBackground(true);
        m_markdownView->setAutoFitHeight(true);
        m_markdownView->setAllowNetworkAccess(true);
        connect(m_markdownView, &ui::widget::MarkdownView::linkActivated, this, &MessageCardWidget::linkActivated);
        connect(m_markdownView, &ui::widget::MarkdownView::autoFitHeightChanged, this, [this](int) {
            m_mainLayout->invalidate();
            updateGeometry();
            emit contentHeightChanged();
        });

        m_mainLayout->addWidget(m_contentRow);

        m_actionBar = new QWidget(this);
        m_actionBar->setFixedHeight(24);
        m_actionBar->installEventFilter(this);
        m_actionLayout = new QHBoxLayout(m_actionBar);
        m_actionLayout->setContentsMargins(0, 0, 0, 0);
        m_actionLayout->setSpacing(4);

        m_copyButton = new fluent::basicinput::Button(this);
        m_copyButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_copyButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_copyButton->setIconGlyph(Typography::Icons::Copy, 13);
        m_copyButton->setToolTip(tr("复制"));
        m_copyButton->setCursor(Qt::PointingHandCursor);
        m_copyButton->setFocusPolicy(Qt::NoFocus);
        m_copyButton->hide();
        connect(m_copyButton, &QPushButton::clicked, this, [this]() {
            QGuiApplication::clipboard()->setText(m_markdownView->markdown());
            });

        m_mainLayout->addWidget(m_actionBar);

        updateVisuals();
    }

    void MessageCardWidget::setMessage(const domain::conversation::Message& message)
    {
        syncMessage(message);
        updateVisuals();
    }

    void MessageCardWidget::setSenderName(const QString& name)
    {
        m_senderName = name;
        updateVisuals();
    }

    void MessageCardWidget::setAvatarVisible(bool visible)
    {
        if (m_avatarVisible == visible) return;
        m_avatarVisible = visible;
        updateVisuals();
    }

    void MessageCardWidget::setHeaderVisible(bool visible)
    {
        if (m_headerVisible == visible) return;
        m_headerVisible = visible;
        updateVisuals();
    }

    bool MessageCardWidget::hasHeightForWidth() const
    {
        return true;
    }

    int MessageCardWidget::heightForWidth(int width) const
    {
        if (width <= 0) return sizeHint().height();
        return qMax(24, m_mainLayout->totalHeightForWidth(width));
    }

    QSize MessageCardWidget::sizeHint() const
    {
        return m_mainLayout ? m_mainLayout->sizeHint() : QWidget::sizeHint();
    }

    QSize MessageCardWidget::minimumSizeHint() const
    {
        return m_mainLayout ? m_mainLayout->minimumSize() : QWidget::minimumSizeHint();
    }

    void MessageCardWidget::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        if (m_message.role == domain::MessageRole::User) {
            const int maxBubbleWidth = qMax(200, static_cast<int>(width() * 0.85));
            if (m_userBubbleCard) m_userBubbleCard->setMaximumWidth(maxBubbleWidth);
            const QMargins margins = m_bubbleLayout ? m_bubbleLayout->contentsMargins() : QMargins(10, 6, 10, 6);
            const int horizontalMargins = margins.left() + margins.right();
            if (m_markdownView) m_markdownView->setPreferredWidthLimit(qMax(0, maxBubbleWidth - horizontalMargins));
        }
    }

    bool MessageCardWidget::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == m_actionBar) {
            if (event->type() == QEvent::Enter) {
                if (m_message.status != domain::MessageStatus::Sending && m_copyButton) {
                    m_copyButton->show();
                }
            } else if (event->type() == QEvent::Leave) {
                if (m_copyButton) {
                    m_copyButton->hide();
                }
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void MessageCardWidget::updateActionBarVisibility()
    {
        const bool isStreaming = (m_message.status == domain::MessageStatus::Sending);
        if (isStreaming) {
            if (m_copyButton) m_copyButton->hide();
        } else {
            if (m_actionBar && m_actionBar->underMouse()) {
                if (m_copyButton) m_copyButton->show();
            } else {
                if (m_copyButton) m_copyButton->hide();
            }
        }
    }

    void MessageCardWidget::onThemeUpdated()
    {
        if (m_markdownView) {
            m_markdownView->onThemeUpdated();
        }
        if (m_processGroupWidget) {
            m_processGroupWidget->onThemeUpdated();
        }
        updateVisuals();
        update();
    }

    ProcessGroupWidget* MessageCardWidget::processGroup()
    {
        if (!m_processGroupWidget) {
            m_processGroupWidget = new ProcessGroupWidget(this);
            connect(m_processGroupWidget, &ProcessGroupWidget::contentHeightChanged, this, [this]() {
                m_mainLayout->invalidate();
                updateGeometry();
                emit contentHeightChanged();
            });

            int index = m_mainLayout->indexOf(m_contentRow);
            if (index >= 0) {
                m_mainLayout->insertWidget(index, m_processGroupWidget);
            }
            else {
                m_mainLayout->addWidget(m_processGroupWidget);
            }
        }
        return m_processGroupWidget;
    }

    void MessageCardWidget::syncMessage(const domain::conversation::Message& newMessage)
    {
        bool isInitialLoad = m_message.id.isNull();
        bool justFinished = (newMessage.status != domain::MessageStatus::Sending && m_message.status == domain::MessageStatus::Sending);
        bool roleChanged = (m_message.role != newMessage.role);

        if (newMessage.status == domain::MessageStatus::Sending && m_message.status != domain::MessageStatus::Sending) {
            m_markdownView->beginStream();
        }

        m_message = newMessage;

        if (roleChanged) {
            updateVisuals();
        }

        QString fullMarkdown;
        bool hasProcess = false;
        qint64 totalDurationMs = 0;

        int thoughtIndex = 0;

        for (const auto& block : m_message.blocks) {
            if (block.isThought()) {
                const auto& thought = std::get<domain::conversation::ThoughtBlock>(block.payload);
                ui::widget::message::blocks::ThinkingBlockWidget* thoughtWidget = nullptr;
                if (thoughtIndex < m_thoughtWidgets.size()) {
                    thoughtWidget = m_thoughtWidgets[thoughtIndex];
                    if (m_message.status == domain::MessageStatus::Sending && thoughtWidget->isStreaming()) {
                        processGroup()->setExpanded(true, false);
                    }
                }
                else {
                    thoughtWidget = new ui::widget::message::blocks::ThinkingBlockWidget(processGroup());
                    processGroup()->addProcessWidget(thoughtWidget);
                    m_thoughtWidgets.append(thoughtWidget);
                    if (m_message.status == domain::MessageStatus::Sending) {
                        thoughtWidget->beginStream();
                        processGroup()->setExpanded(true, false);
                    }
                }
                bool wasStreaming = thoughtWidget->isStreaming();
                thoughtWidget->setThought(thought.thought);
                thoughtWidget->setDurationMs(thought.durationMs);
                if ((thought.durationMs > 0 || m_message.status != domain::MessageStatus::Sending) && wasStreaming) {
                    thoughtWidget->finishStream();
                }
                totalDurationMs += thought.durationMs;
                hasProcess = true;
                thoughtIndex++;
            }
            else if (block.isToolCall()) {
                const auto& tc = std::get<domain::conversation::ToolCallBlock>(block.payload);
                for (const auto& call : tc.calls) {
                    ui::widget::message::blocks::AbstractToolBlockWidget* toolWidget = nullptr;
                    if (m_toolWidgets.contains(call.id)) {
                        toolWidget = m_toolWidgets[call.id];
                    }
                    else {
                        toolWidget = ui::widget::message::blocks::ToolWidgetFactory::create(call, processGroup());
                        processGroup()->addProcessWidget(toolWidget);
                        m_toolWidgets[call.id] = toolWidget;
                    }
                    if (m_message.status == domain::MessageStatus::Sending) {
                        processGroup()->setExpanded(true, false);
                    }
                    hasProcess = true;
                }
            }
            else if (block.isToolResult()) {
                const auto& tr = std::get<domain::conversation::ToolResultBlock>(block.payload);
                for (const auto& res : tr.results) {
                    ui::widget::message::blocks::AbstractToolBlockWidget* toolWidget = nullptr;
                    if (m_toolWidgets.contains(res.toolCallId)) {
                        toolWidget = m_toolWidgets[res.toolCallId];
                        toolWidget->setToolResult(res);
                    }
                    else {
                        domain::agent::ToolCall dummyCall{ res.toolCallId, QStringLiteral("tool_result"), {} };
                        toolWidget = ui::widget::message::blocks::ToolWidgetFactory::create(dummyCall, processGroup());
                        toolWidget->setToolResult(res);
                        processGroup()->addProcessWidget(toolWidget);
                        m_toolWidgets[res.toolCallId] = toolWidget;
                    }
                    if (m_message.status == domain::MessageStatus::Sending) {
                        processGroup()->setExpanded(true, false);
                    }
                    hasProcess = true;
                }
            }
            else if (block.isImage()) {
            }
            else if (block.isText()) {
                const auto& tb = std::get<domain::conversation::TextBlock>(block.payload);
                fullMarkdown += tb.text;
            }
        }

        if (hasProcess) {
            if (totalDurationMs > 0) {
                processGroup()->setDurationMs(totalDurationMs);
            }
            else {
                processGroup()->setTitle(m_message.status == domain::MessageStatus::Sending ? QStringLiteral("思考中...") : QStringLiteral("已处理"));
            }

            if (justFinished) {
                if (m_processGroupWidget) {
                    m_processGroupWidget->setExpanded(false);
                }
            }
            else if (isInitialLoad && m_message.status != domain::MessageStatus::Sending) {
                if (m_processGroupWidget) {
                    m_processGroupWidget->setExpanded(false, false);
                }
            }
        }

        if (newMessage.role == domain::MessageRole::User) {
            m_markdownView->setHorizontalSizingMode(ui::widget::MarkdownView::HorizontalSizingMode::FitContent);
            m_markdownView->setContentMargins(QMarginsF(0, 0, 0, 0));
            const int availableCardWidth = width() > 100 ? width() : 800;
            const int maxBubbleWidth = qMax(200, static_cast<int>(availableCardWidth * 0.85));
            const QMargins margins = m_bubbleLayout ? m_bubbleLayout->contentsMargins() : QMargins(10, 6, 10, 6);
            const int horizontalMargins = margins.left() + margins.right();
            m_markdownView->setPreferredWidthLimit(qMax(0, maxBubbleWidth - horizontalMargins));
        } else {
            m_markdownView->setHorizontalSizingMode(ui::widget::MarkdownView::HorizontalSizingMode::FillAvailable);
            m_markdownView->setContentMargins(QMarginsF(0, 2, 0, 2));
            m_markdownView->setPreferredWidthLimit(0);
        }

        if (m_markdownView->markdown() != fullMarkdown) {
            if (m_message.status == domain::MessageStatus::Sending && fullMarkdown.startsWith(m_markdownView->markdown()) && m_markdownView->isStreaming()) {
                m_markdownView->appendMarkdown(fullMarkdown.mid(m_markdownView->markdown().length()));
            }
            else {
                m_markdownView->setMarkdown(fullMarkdown);
            }
        }

        if (m_message.status != domain::MessageStatus::Sending && m_markdownView->isStreaming()) {
            m_markdownView->finishStream();
        }

        if (m_message.status == domain::MessageStatus::Failed && !m_message.errorMessage.isEmpty()) {
            if (!m_errorWidget) {
                m_errorWidget = new ui::widget::message::blocks::ErrorBlockWidget(
                    QStringLiteral("生成或执行失败"),
                    m_message.errorMessage,
                    processGroup()
                );
                processGroup()->addProcessWidget(m_errorWidget);
            }
            if (m_message.status != domain::MessageStatus::Sending) {
                processGroup()->setExpanded(false);
            }
        }
        else if (m_errorWidget) {
            m_errorWidget->hide();
        }

        m_mainLayout->activate();
        updateActionBarVisibility();
    }

    void MessageCardWidget::resetForReuse()
    {
        if (m_processGroupWidget) {
            m_mainLayout->removeWidget(m_processGroupWidget);
            m_processGroupWidget->deleteLater();
            m_processGroupWidget = nullptr;
        }
        m_thoughtWidgets.clear();
        m_toolWidgets.clear();
        m_errorWidget = nullptr;
        m_message = {};
        if (m_markdownView) {
            m_markdownView->clear();
            m_markdownView->setPreferredWidthLimit(0);
            m_markdownView->setContentMargins(QMarginsF(0, 2, 0, 2));
        }
        updateVisuals();
    }

    void MessageCardWidget::appendError(const QString& summary, const QString& details)
    {
        auto* errorBlock = new ui::widget::message::blocks::ErrorBlockWidget(summary, details, processGroup());
        processGroup()->addProcessWidget(errorBlock);
    }

    void MessageCardWidget::updateVisuals()
    {
        const auto& colors = themeColorsRef();
        const bool isUser = (m_message.role == domain::MessageRole::User);

        m_avatar->setVisible(m_avatarVisible);
        if (isUser) {
            m_avatar->setName(tr("我"));
            m_avatar->setInitials(QStringLiteral("U"));
            m_avatar->setBackgroundColor(colors.bgLayerAlt);
            m_avatar->setForegroundColor(colors.textPrimary);
        }
        else {
            m_avatar->setName(tr("ForgeAI"));
            m_avatar->setInitials(QStringLiteral("AI"));
            m_avatar->setBackgroundColor(colors.accentDefault);
            m_avatar->setForegroundColor(Qt::white);
        }
        if (!isUser) {
            m_senderLabel->setText(!m_senderName.isEmpty() ? m_senderName : tr("ForgeAI"));
        }
        m_timeLabel->setText(formatTime(m_message.createdAt));

        const bool structureChanged = !m_visualsConstructed
            || (m_currentVisualRole != m_message.role)
            || (m_currentVisualAvatarVisible != m_avatarVisible)
            || (m_currentVisualHeaderVisible != m_headerVisible);

        if (structureChanged) {
            m_visualsConstructed = true;
            m_currentVisualRole = m_message.role;
            m_currentVisualAvatarVisible = m_avatarVisible;
            m_currentVisualHeaderVisible = m_headerVisible;

            while (m_headerLayout->count() > 0) m_headerLayout->takeAt(0);
            while (m_contentRowLayout->count() > 0) m_contentRowLayout->takeAt(0);
            while (m_actionLayout->count() > 0) m_actionLayout->takeAt(0);
            while (m_bubbleLayout->count() > 0) m_bubbleLayout->takeAt(0);

            if (isUser) {
                m_headerWidget->hide();

                m_bubbleLayout->addWidget(m_markdownView);
                m_userBubbleCard->show();

                m_contentRowLayout->addStretch(1);
                m_contentRowLayout->addWidget(m_userBubbleCard, 0, Qt::AlignRight | Qt::AlignTop);
                m_contentRowLayout->addWidget(m_avatar, 0, Qt::AlignTop);

                m_actionLayout->addStretch(1);
                m_actionLayout->addWidget(m_timeLabel);
                m_actionLayout->addWidget(m_copyButton);
                if (m_avatarVisible) {
                    m_actionLayout->addSpacing(32);
                }
            }
            else {
                m_headerWidget->setVisible(m_headerVisible);

                m_headerLayout->addWidget(m_avatar);
                m_headerLayout->addWidget(m_senderLabel);
                m_headerLayout->addWidget(m_timeLabel);
                m_headerLayout->addStretch(1);

                m_userBubbleCard->hide();
                m_contentRowLayout->addWidget(m_markdownView, 1);

                m_actionLayout->addWidget(m_copyButton);
                m_actionLayout->addStretch(1);
            }
        }

        if (isUser) {
            m_markdownView->setHorizontalSizingMode(ui::widget::MarkdownView::HorizontalSizingMode::FitContent);
            m_markdownView->setContentMargins(QMarginsF(0, 0, 0, 0));
            const int availableCardWidth = width() > 100 ? width() : 800;
            const int maxBubbleWidth = qMax(200, static_cast<int>(availableCardWidth * 0.85));
            m_userBubbleCard->setMaximumWidth(maxBubbleWidth);
            const QMargins margins = m_bubbleLayout ? m_bubbleLayout->contentsMargins() : QMargins(10, 6, 10, 6);
            const int horizontalMargins = margins.left() + margins.right();
            m_markdownView->setPreferredWidthLimit(qMax(0, maxBubbleWidth - horizontalMargins));
        }
        else {
            m_markdownView->setHorizontalSizingMode(ui::widget::MarkdownView::HorizontalSizingMode::FillAvailable);
            m_markdownView->setContentMargins(QMarginsF(0, 2, 0, 2));
            m_markdownView->setPreferredWidthLimit(0);
            m_headerWidget->setVisible(m_headerVisible);
        }

        updateActionBarVisibility();
    }

} // namespace ui::widget::message
