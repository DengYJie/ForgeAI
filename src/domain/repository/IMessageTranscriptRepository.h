#pragma once
#include <QList>
#include <QUuid>
#include "domain/conversation/Message.h"

namespace domain::repository {
    /**
     * @brief 消息内容与轨迹日志仓储接口（通常基于本地 JSONL 纯追加流式存储）
     * @note 仅在进入具体聊天页面、大模型流式输出落盘或导出轨迹时调用。
     */
    class IMessageTranscriptRepository {
    public:
        virtual ~IMessageTranscriptRepository() = default;

        /**
         * @brief 将单条消息以 Append-only 方式极速追加到底层存储
         */
        virtual void appendMessage(const QUuid &conversationId, const domain::conversation::Message &message) = 0;

        /**
         * @brief 获取指定会话的完整历史消息列表（按时间正序）
         */
        virtual QList<domain::conversation::Message> getMessagesByConversationId(const QUuid &conversationId) = 0;

        /**
         * @brief 获取指定交互回合的消息列表
         */
        virtual QList<domain::conversation::Message> getMessagesByTurnId(const QUuid &turnId) = 0;

        /**
         * @brief 清理指定会话的物理日志文件
         */
        virtual void deleteTranscript(const QUuid &conversationId) = 0;
    };
} // namespace domain::repository
