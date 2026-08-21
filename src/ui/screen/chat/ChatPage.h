#pragma once

#include "ui/base/BasePage.h"

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::widget {
    class CollapsibleSplitView;
}

namespace ui::screen::chat {
    class ChatSidebar;
    class ChatHeader;
    class ChatAnchorBar;
    class ChatInputBox;
    class ChatViewModel;
    struct ChatState;

    class ChatPage : public ui::base::BasePage {
        Q_OBJECT

    public:
        explicit ChatPage(QWidget *parent = nullptr);

        ~ChatPage() override = default;

    private:
        void setupUi();

        void setupViewModel();

        void render(const ChatState &state);

        ChatViewModel *m_viewModel = nullptr;
        QVBoxLayout *m_rootLayout = nullptr;

        ui::widget::CollapsibleSplitView *m_splitView = nullptr;
        ChatSidebar *m_sidebar = nullptr;
        QWidget *m_chatAreaWidget = nullptr;

        ChatHeader *m_header = nullptr;
        ChatAnchorBar *m_anchorBar = nullptr;
        QWidget *m_mainArea = nullptr;
        QWidget *m_mainRight = nullptr;
        ChatInputBox *m_inputBox = nullptr;
    };
} // namespace ui::screen::chat
