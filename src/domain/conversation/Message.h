#pragma once
#include <QUuid>
#include <QList>
#include <QDateTime>
#include "domain/Types.h"
#include "domain/conversation/MessageBlock.h"

namespace domain::conversation {
    /**
     * @brief 消息实体（属于某个回合的具体记录）
     */
    struct Message {
        QUuid id; ///< 消息全局唯一标识符
        QUuid turnId; ///< 所属的交互回合 (Turn) ID

        domain::MessageRole role; ///< 发送者角色 (System, User, Assistant, Tool)
        domain::MessageStatus status; ///< 当前消息的传输/生成状态

        QList<MessageBlock> blocks; ///< 消息包含的内容块列表（支持文本、思考流、多模态附件、工具调用）

        QDateTime createdAt; ///< 消息创建时间
    };
} // namespace domain::conversation
