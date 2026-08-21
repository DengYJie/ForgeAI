#include "ChatViewModel.h"

namespace ui::screen::chat {
    ChatViewModel::ChatViewModel(QObject *parent)
        : BaseViewModel<ChatViewModel, ChatState>(parent) {
    }

    ChatViewModel::~ChatViewModel() = default;

    void ChatViewModel::setInputMessage(const QString &message) {
        updateState([message](ChatState &s) { s.inputMessage = message; });
    }

    void ChatViewModel::sendMessage() {
        if (m_state.inputMessage.trimmed().isEmpty()) return;

        updateState([](ChatState &s) {
            s.isGenerating = true;
            s.statusMessage = QStringLiteral("正在思考中...");
            s.inputMessage.clear();
        });
    }

    void ChatViewModel::stopGeneration() {
        updateState([](ChatState &s) {
            s.isGenerating = false;
            s.statusMessage.clear();
        });
    }

    void ChatViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::chat
