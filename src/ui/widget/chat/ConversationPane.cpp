#include "ConversationPane.h"
#include "ChatHeader.h"
#include "ChatAnchorBar.h"
#include "ChatInputBox.h"
#include "ui/widget/message/MessageListView.h"
#include <FluentQt/Design.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/Scrolling.h>
#include <components/scrolling/OverlayScrollChrome.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>

namespace ui::widget::chat {

namespace {
constexpr int kMinContentWidth = 320;
constexpr int kMaxContentWidth = 1000;
} // namespace

ConversationPane::ConversationPane(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ConversationPane::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_header = new ChatHeader(this);
    mainLayout->addWidget(m_header);

    m_contentRow = new QWidget(this);
    m_contentRow->installEventFilter(this);
    m_contentRowLayout = new QHBoxLayout(m_contentRow);
    m_contentRowLayout->setContentsMargins(0, 0, 0, 0);
    m_contentRowLayout->setSpacing(8);

    m_anchorBar = new ChatAnchorBar(m_contentRow);
    m_anchorBar->hide();
    m_contentRowLayout->addWidget(m_anchorBar, 0);

    m_conversationColumn = new QWidget(m_contentRow);
    m_conversationColumn->setMinimumWidth(kMinContentWidth);
    m_conversationColumn->setMaximumWidth(kMaxContentWidth);

    auto* conversationLayout = new QVBoxLayout(m_conversationColumn);
    conversationLayout->setContentsMargins(0, 0, 0, 16);
    conversationLayout->setSpacing(6);

    auto* messageSurface = new QWidget(m_conversationColumn);
    auto* messageStack = new QStackedLayout(messageSurface);
    messageStack->setContentsMargins(0, 0, 0, 0);
    messageStack->setStackingMode(QStackedLayout::StackAll);

    m_messageList = new message::MessageListView(messageSurface);
    m_messageList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_messageList->setVerticalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
    messageStack->addWidget(m_messageList);

    m_emptyStateLabel = new fluent::textfields::Label(messageSurface);
    m_emptyStateLabel->setFluentTypography(Typography::FontRole::Body);
    m_emptyStateLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setWordWrap(true);
    m_emptyStateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyStateLabel->hide();
    messageStack->addWidget(m_emptyStateLabel);

    conversationLayout->addWidget(messageSurface, 1);

    auto* inputContainer = new QWidget(m_conversationColumn);
    auto* inputLayout = new QVBoxLayout(inputContainer);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(4);

    m_inputBox = new ChatInputBox(inputContainer);
    m_inputBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    inputLayout->addWidget(m_inputBox);

    m_statusLabel = new fluent::textfields::Label(inputContainer);
    m_statusLabel->setFluentTypography(Typography::FontRole::Caption);
    m_statusLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    m_statusLabel->setAlignment(Qt::AlignHCenter);
    m_statusLabel->hide();
    inputLayout->addWidget(m_statusLabel);

    conversationLayout->addWidget(inputContainer, 0);

    m_contentRowLayout->addWidget(m_conversationColumn, 1, Qt::AlignHCenter);

    m_rightBalanceSpacer = new QWidget(m_contentRow);
    m_rightBalanceSpacer->setFixedWidth(32);
    m_rightBalanceSpacer->hide();
    m_contentRowLayout->addWidget(m_rightBalanceSpacer, 0);

    auto* internalBar = m_messageList->verticalScrollBar();
    m_externalScrollBar = fluent::scrolling::createOverlayScrollBar(
        Qt::Vertical, m_contentRow, internalBar, QStringLiteral("conversationOverlayScrollBar")
    );

    connect(internalBar, &QScrollBar::rangeChanged, this, [this, internalBar]() {
        fluent::scrolling::mirrorNativeScrollBar(m_externalScrollBar, internalBar);
        updateScrollBarGeometry();
    });

    mainLayout->addWidget(m_contentRow, 1);
}

void ConversationPane::updateScrollBarGeometry() {
    if (m_externalScrollBar && m_contentRow) {
        fluent::scrolling::placeVerticalScrollBar(
            m_externalScrollBar,
            m_contentRow->rect(),
            /*top=*/0,
            /*rightInset=*/0,
            /*bottomInset=*/0
        );
    }
}

bool ConversationPane::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_contentRow && event->type() == QEvent::Resize) {
        updateScrollBarGeometry();
    }
    return QWidget::eventFilter(watched, event);
}

void ConversationPane::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateScrollBarGeometry();
}

void ConversationPane::setAnchorBarVisible(bool visible) {
    m_anchorBar->setVisible(visible);
    if (m_rightBalanceSpacer) {
        m_rightBalanceSpacer->setVisible(visible);
    }
}

void ConversationPane::setEmptyStateVisible(bool visible) {
    m_emptyStateLabel->setVisible(visible);
}

void ConversationPane::setStatusLabelVisible(bool visible) {
    m_statusLabel->setVisible(visible);
}

} // namespace ui::widget::chat
