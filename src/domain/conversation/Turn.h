#pragma once
#include <QUuid>
#include <QString>
#include <QDateTime>
#include "domain/Types.h"

namespace domain::conversation {
    /**
     * @brief 交互回合实体（代表一轮完整的用户输入与智能体多步执行/回答过程）
     */
    struct Turn {
        QUuid id; ///< 回合全局唯一标识符
        QUuid conversationId; ///< 所属会话 ID

        domain::TurnStatus status; ///< 当前回合的执行状态
        QString errorMessage; ///< 失败时的详细错误日志（成功时为空）

        QDateTime createdAt; ///< 回合开始时间
        QDateTime updatedAt; ///< 回合最后更新时间
    };
} // namespace domain::conversation
