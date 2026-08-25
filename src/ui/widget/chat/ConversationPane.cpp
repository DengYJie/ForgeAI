#include "ConversationPane.h"
#include "ChatHeader.h"
#include "ChatAnchorBar.h"
#include "ChatInputBox.h"
#include "ui/widget/message/MessageListView.h"
#include <FluentQt/Design.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>

namespace ui::widget::chat {

namespace {
constexpr int kHorizontalMargin = 16;
constexpr int kMinBoxWidth = 200;
constexpr int kMaxBoxWidth = 1000;

class TransparentOverlay : public QWidget {
public:
    using QWidget::QWidget;
protected:
    void mousePressEvent(QMouseEvent* event) override { event->ignore(); }
    void mouseReleaseEvent(QMouseEvent* event) override { event->ignore(); }
    void mouseMoveEvent(QMouseEvent* event) override { event->ignore(); }
    void wheelEvent(QWheelEvent* event) override { event->ignore(); }
};
} // namespace

ConversationPane::ConversationPane(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ConversationPane::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Header
    m_header = new ChatHeader(this);
    mainLayout->addWidget(m_header);

    // 2. Main Row: AnchorBar (left) + Conversation Column (center)
    auto* contentRow = new QWidget(this);
    m_contentRowLayout = new QHBoxLayout(contentRow);
    m_contentRowLayout->setContentsMargins(8, 6, 8, 0);
    m_contentRowLayout->setSpacing(8);

    m_anchorBar = new ChatAnchorBar(contentRow);
    m_anchorBar->hide(); // Hidden by default, enabled by ChatPage
    m_contentRowLayout->addWidget(m_anchorBar, 0);

    auto* conversationColumn = new QWidget(contentRow);
    auto* columnStack = new QStackedLayout(conversationColumn);
    columnStack->setContentsMargins(0, 0, 0, 0);
    columnStack->setStackingMode(QStackedLayout::StackAll);

    // 2.1 Message Surface (Full height from top to bottom)
    auto* messageSurface = new QWidget(conversationColumn);
    auto* messageStack = new QStackedLayout(messageSurface);
    messageStack->setContentsMargins(0, 0, 0, 0);
    messageStack->setStackingMode(QStackedLayout::StackAll);

    m_messageList = new message::MessageListView(messageSurface);
    m_messageList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    messageStack->addWidget(m_messageList);

    m_emptyStateLabel = new fluent::textfields::Label(messageSurface);
    m_emptyStateLabel->setFluentTypography(Typography::FontRole::Body);
    m_emptyStateLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setWordWrap(true);
    m_emptyStateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyStateLabel->hide();
    messageStack->addWidget(m_emptyStateLabel);

    columnStack->addWidget(messageSurface);

    // 2.2 Bottom Input Area floating at the bottom
    auto* inputOverlay = new TransparentOverlay(conversationColumn);
    auto* inputLayout = new QVBoxLayout(inputOverlay);
    inputLayout->setContentsMargins(0, 0, 0, 16);
    inputLayout->setSpacing(4);
    inputLayout->addStretch(1);

    m_inputBox = new ChatInputBox(inputOverlay);
    inputLayout->addWidget(m_inputBox, 0, Qt::AlignHCenter);

    m_statusLabel = new fluent::textfields::Label(inputOverlay);
    m_statusLabel->setFluentTypography(Typography::FontRole::Caption);
    m_statusLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    m_statusLabel->setAlignment(Qt::AlignHCenter);
    m_statusLabel->hide();
    inputLayout->addWidget(m_statusLabel);

    columnStack->addWidget(inputOverlay);

    m_contentRowLayout->addWidget(conversationColumn, 1);
    mainLayout->addWidget(contentRow, 1);
}

void ConversationPane::setAnchorBarVisible(bool visible) {
    m_anchorBar->setVisible(visible);
    if (m_contentRowLayout) {
        // When AnchorBar (32px + 8px spacing) is visible on the left, add 40px right margin to balance symmetrically
        m_contentRowLayout->setContentsMargins(visible ? 4 : 8, 6, visible ? 44 : 8, 0);
    }
    updateInputBoxWidth();
}

void ConversationPane::setEmptyStateVisible(bool visible) {
    m_emptyStateLabel->setVisible(visible);
}

void ConversationPane::setStatusLabelVisible(bool visible) {
    m_statusLabel->setVisible(visible);
}

void ConversationPane::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateInputBoxWidth();
}

void ConversationPane::updateInputBoxWidth() {
    if (!m_inputBox) return;
    const bool anchorVisible = (m_anchorBar && m_anchorBar->isVisible());
    const int sideWidths = anchorVisible ? (2 * (8 + 32 + 8)) : (2 * 8);
    const int columnWidth = width() - sideWidths;
    if (columnWidth <= 0) return;

    const int cardWidth = columnWidth - 2 * kHorizontalMargin;
    const int maxAllowedWidth = qMin(columnWidth, kMaxBoxWidth);
    const int minAllowedWidth = qMin(kMinBoxWidth, maxAllowedWidth);
    const int targetWidth = qBound(minAllowedWidth, cardWidth, maxAllowedWidth);
    if (targetWidth > 0 && m_inputBox->width() != targetWidth) {
        m_inputBox->setFixedWidth(targetWidth);
    }
}

} // namespace ui::widget::chat
