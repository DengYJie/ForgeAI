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

ConversationPane::ConversationPane(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ConversationPane::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 16);
    mainLayout->setSpacing(0);

    // 1. Header
    m_header = new ChatHeader(this);
    mainLayout->addWidget(m_header);

    // 2. Middle Area
    auto* middleRow = new QWidget(this);
    auto* middleLayout = new QHBoxLayout(middleRow);
    middleLayout->setContentsMargins(4, 6, 8, 6);
    middleLayout->setSpacing(8);

    m_anchorBar = new ChatAnchorBar(middleRow);
    m_anchorBar->hide(); // Hidden by default, enabled by ChatPage
    middleLayout->addWidget(m_anchorBar, 0);

    auto* messageSurface = new QWidget(middleRow);
    auto* messageStack = new QStackedLayout(messageSurface);
    messageStack->setContentsMargins(20, 0, 16, 0);
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

    middleLayout->addWidget(messageSurface, 1);
    mainLayout->addWidget(middleRow, 1);

    // 3. Bottom Input Area
    auto* inputContainer = new QWidget(this);
    auto* inputLayout = new QVBoxLayout(inputContainer);
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

    mainLayout->addWidget(inputContainer);
}

void ConversationPane::setAnchorBarVisible(bool visible) {
    m_anchorBar->setVisible(visible);
}

void ConversationPane::setEmptyStateVisible(bool visible) {
    m_emptyStateLabel->setVisible(visible);
}

void ConversationPane::setStatusLabelVisible(bool visible) {
    m_statusLabel->setVisible(visible);
}

} // namespace ui::widget::chat
