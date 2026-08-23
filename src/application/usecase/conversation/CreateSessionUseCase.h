#pragma once

#include <QObject>
#include <QList>
#include "ui/screen/chat/ChatSessionListModel.h"

namespace domain::service {
    class IConversationService;
}

namespace application::usecase::conversation {
    /**
     * @brief 会话创建或复用用例（遵循 Cherry Studio 语义）
     */
    class CreateSessionUseCase : public QObject {
        Q_OBJECT

    public:
        explicit CreateSessionUseCase(
            domain::service::IConversationService *conversationService,
            QObject *parent = nullptr
        );

        ~CreateSessionUseCase() override = default;

        /**
         * @brief 执行会话创建/复用
         * @param sessions 会话列表快照引用（会被就地更新）
         * @param currentSessionId 当前会话 ID
         * @return 激活的目标会话 ID
         */
        QString execute(
            QList<ui::screen::chat::ChatSessionItemData> &sessions,
            const QString &currentSessionId
        );

    private:
        domain::service::IConversationService *m_conversationService = nullptr;
    };
} // namespace application::usecase::conversation
