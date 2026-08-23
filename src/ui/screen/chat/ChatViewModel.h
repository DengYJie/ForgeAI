#pragma once

#include "ui/base/BaseViewModel.h"
#include "domain/conversation/Message.h"
#include "ui/widget/chat/ChatAnchorBar.h"
#include <QString>
#include <QList>
#include <QUuid>

namespace ui::screen::chat {
    struct ChatState {
        QString currentSessionId;
        QString sessionTitle = QStringLiteral("新对话");
        QString currentModelName = QStringLiteral("DeepSeek-R1");
        bool isGenerating = false;
        QString statusMessage;

        // 核心领域实体列表（单向权威数据源）
        QList<domain::conversation::Message> messages;

        // 派生的时间线锚点列表与当前激活索引
        QList<ui::widget::chat::ChatAnchorItem> anchors;
        int activeAnchorIndex = -1;

        bool operator==(const ChatState &other) const = default;
    };

    class ChatViewModel : public BaseViewModel<ChatViewModel, ChatState> {
        Q_OBJECT

    public:
        explicit ChatViewModel(QObject *parent = nullptr);

        ~ChatViewModel() override;

        void loadSession(const QString &sessionId);

        void newSession();

        void sendMessage(const QString &text);

        void stopGeneration();

        void setModelName(const QString &modelName);

        void setActiveAnchorByMessageId(const QUuid &messageId);

        void setActiveAnchorIndex(int index);

    Q_SIGNALS:
        void stateChanged(const ui::screen::chat::ChatState &state);

    protected:
        void emitStateChanged() override;

    private:
        static void recalculateAnchors(ChatState &s);
    };
} // namespace ui::screen::chat
