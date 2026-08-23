#pragma once

#include "ui/base/BaseViewModel.h"
#include "domain/conversation/Message.h"
#include "ui/widget/chat/ChatAnchorBar.h"
#include "ChatSessionListModel.h"
#include "application/usecase/chat/ChatUseCases.h"
#include <QString>
#include <QList>
#include <QUuid>

namespace ui::screen::chat {
    struct ChatState {
        // 会话列表
        QList<ChatSessionItemData> sessions;
        QString currentSessionId;
        QString sessionTitle = QStringLiteral("新对话");
        bool sessionTitleManuallyEdited = false; ///< 用户是否手动改过标题，若是则首条消息不覆盖

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
        explicit ChatViewModel(
            const application::usecase::chat::ChatUseCases &useCases,
            QObject *parent = nullptr
        );

        ~ChatViewModel() override;

        void loadSession(const QString &sessionId);

        void newSession();

        void deleteSession(const QString &sessionId);

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
        void setupUseCaseConnections();

        static void recalculateAnchors(ChatState &s);

        // 在会话列表中同步更新指定 session 的 title
        static void syncSessionTitle(ChatState &s, const QString &sessionId, const QString &title);

        application::usecase::chat::ChatUseCases m_useCases;
    };
} // namespace ui::screen::chat
