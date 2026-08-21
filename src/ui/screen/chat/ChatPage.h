#pragma once

#include "ui/base/BasePage.h"

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::screen::chat {
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
        fluent::textfields::Label *m_titleLabel = nullptr;
        fluent::textfields::Label *m_subtitleLabel = nullptr;
    };
} // namespace ui::screen::chat
