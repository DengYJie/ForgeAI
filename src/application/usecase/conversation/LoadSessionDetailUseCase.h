#pragma once

#include <QObject>
#include <QList>
#include "domain/conversation/Message.h"

namespace domain::service {
    class IConversationService;
}

namespace application::usecase::conversation {
    /**
     * @brief 加载指定会话消息历史流用例
     * @details 根据会话唯一标识拉取该会话下的所有历史消息实体（包含文本、思维链、工具调用等块数据）。
     */
    class LoadSessionDetailUseCase : public QObject {
        Q_OBJECT

    public:
        explicit LoadSessionDetailUseCase(
            domain::service::IConversationService *conversationService,
            QObject *parent = nullptr
        );

        ~LoadSessionDetailUseCase() override = default;

        /**
         * @brief 执行加载会话消息流
         * @param sessionId 目标会话 ID
         * @return 消息实体列表
         */
        QList<domain::conversation::Message> execute(const QString &sessionId);

    private:
        domain::service::IConversationService *m_conversationService = nullptr;
    };
} // namespace application::usecase::conversation
