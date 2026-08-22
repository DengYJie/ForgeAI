#pragma once
#include <QList>
#include <QUuid>
#include <optional>
#include "domain/conversation/Conversation.h"
#include "domain/conversation/Turn.h"

namespace domain::repository {
    /**
     * @brief 会话元数据仓储接口（负责管理 Conversation 和 Turn 的生命周期）
     * @note 侧边栏和主页只需依赖此接口，完全不需要加载庞大的消息日志，保证极致性能。
     */
    class IConversationRepository {
    public:
        virtual ~IConversationRepository() = default;

        virtual QList<domain::conversation::Conversation> getAllConversations() = 0;

        virtual std::optional<domain::conversation::Conversation> getConversation(const QUuid &id) = 0;

        virtual void saveConversation(const domain::conversation::Conversation &conversation) = 0;

        virtual void deleteConversation(const QUuid &id) = 0;

        virtual QList<domain::conversation::Turn> getTurnsByConversationId(const QUuid &conversationId) = 0;

        virtual void saveTurn(const domain::conversation::Turn &turn) = 0;
    };
} // namespace domain::repository
