#pragma once
#include <QUuid>
#include <QList>
#include <QDateTime>
#include "domain/Types.h"
#include "domain/conversation/MessageBlock.h"

#include <QJsonObject>
#include <QString>

namespace domain::conversation {
    /**
     * @brief 消息实体
     */
    struct Message {
        QUuid id; ///< 消息全局唯一标识符
        QUuid parentId; ///< 父消息 ID，用于追溯对话链或分支

        domain::MessageRole role = domain::MessageRole::User; ///< 发送者角色 (System, User, Assistant, Tool)
        domain::MessageStatus status = domain::MessageStatus::Sent; ///< 当前消息的传输/生成状态
        QString errorMessage; ///< 若 status == Failed，记录具体的失败或网络超时原因

        QList<MessageBlock> blocks; ///< 消息包含的内容块列表（支持文本、思考流、多模态附件、工具调用）

        QJsonObject turnOptions; ///< 快照冻结生成该条消息时的上下文参数（如模型ID、Temperature、启用的工具等）

        QDateTime createdAt; ///< 消息创建时间
    };
} // namespace domain::conversation
