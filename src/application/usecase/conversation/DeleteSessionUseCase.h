#pragma once

#include <QObject>
#include <QList>
#include "ui/screen/chat/ChatSessionListModel.h"

namespace domain::service {
    class IConversationService;
}

namespace application::usecase::conversation {
    /**
     * @brief 删除会话并计算邻近回退用例
     */
    class DeleteSessionUseCase : public QObject {
        Q_OBJECT

    public:
        explicit DeleteSessionUseCase(
            domain::service::IConversationService *conversationService,
            QObject *parent = nullptr
        );

        ~DeleteSessionUseCase() override = default;

        /**
         * @brief 执行删除并回退
         * @param sessions 会话列表快照引用（会被就地更新）
         * @param sessionId 待删除的会话 ID
         * @param currentSessionId 当前激活的会话 ID
         * @return 删除后应激活的会话 ID
         */
        QString execute(
            QList<ui::screen::chat::ChatSessionItemData> &sessions,
            const QString &sessionId,
            const QString &currentSessionId
        );

    private:
        domain::service::IConversationService *m_conversationService = nullptr;
    };
} // namespace application::usecase::conversation
