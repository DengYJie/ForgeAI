#pragma once

#include "ui/base/BaseViewModel.h"
#include <QString>

namespace ui::screen::chat {
    struct ChatState {
        QString inputMessage;
        bool isGenerating = false;
        QString statusMessage;

        bool operator==(const ChatState &other) const = default;
    };

    class ChatViewModel : public BaseViewModel<ChatViewModel, ChatState> {
        Q_OBJECT

    public:
        explicit ChatViewModel(QObject *parent = nullptr);

        ~ChatViewModel() override;

        void setInputMessage(const QString &message);

        void sendMessage();

        void stopGeneration();

    Q_SIGNALS:
        void stateChanged(const ui::screen::chat::ChatState &state);

    protected:
        void emitStateChanged() override;
    };
} // namespace ui::screen::chat
