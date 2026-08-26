#pragma once

#include <QWidget>

class QVBoxLayout;
class QHBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace fluent::scrolling {
    class ScrollBar;
}

namespace ui::widget::message {
    class MessageListView;
}

namespace ui::widget::chat {

class ChatHeader;
class ChatAnchorBar;
class ChatInputBox;

class ConversationPane : public QWidget {
    Q_OBJECT

public:
    explicit ConversationPane(QWidget* parent = nullptr);

    ChatHeader* header() const { return m_header; }
    ChatAnchorBar* anchorBar() const { return m_anchorBar; }
    message::MessageListView* messageList() const { return m_messageList; }
    ChatInputBox* inputBox() const { return m_inputBox; }
    fluent::textfields::Label* emptyStateLabel() const { return m_emptyStateLabel; }
    fluent::textfields::Label* statusLabel() const { return m_statusLabel; }

    void setAnchorBarVisible(bool visible);
    void setEmptyStateVisible(bool visible);
    void setStatusLabelVisible(bool visible);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void updateScrollBarGeometry();

    ChatHeader* m_header = nullptr;
    QWidget* m_contentRow = nullptr;
    QHBoxLayout* m_contentRowLayout = nullptr;
    ChatAnchorBar* m_anchorBar = nullptr;
    QWidget* m_conversationColumn = nullptr;
    message::MessageListView* m_messageList = nullptr;
    ChatInputBox* m_inputBox = nullptr;
    fluent::textfields::Label* m_emptyStateLabel = nullptr;
    fluent::textfields::Label* m_statusLabel = nullptr;
    QWidget* m_rightBalanceSpacer = nullptr;
    fluent::scrolling::ScrollBar* m_externalScrollBar = nullptr;
};

} // namespace ui::widget::chat
