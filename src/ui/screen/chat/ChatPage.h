#pragma once

#include <QWidget>
#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::screen::chat {
    class ChatViewModel;
    struct ChatState;

    class ChatPage : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
        Q_OBJECT

    public:
        explicit ChatPage(QWidget *parent = nullptr);

        ~ChatPage() override = default;

        void onThemeUpdated() override;

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
