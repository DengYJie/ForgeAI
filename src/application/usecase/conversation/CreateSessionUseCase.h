#pragma once

#include <QObject>
#include <QList>
#include "ui/screen/chat/ChatSessionListModel.h"

namespace domain::service {
    class IConversationService;
}

namespace application::usecase::conversation {
    /**
     * @brief 会话创建与复用用例
     * @details 检查现有列表中是否存在空白未命名会话，若有则直接复用切换；若无则创建全新会话并置顶。
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
         * @brief 执行会话创建/复用逻辑
         * @param sessions 会话列表快照引用（新建时会被就地插入新条目）
         * @param currentSessionId 当前激活的会话 ID
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
