#pragma once
#include <QUuid>
#include <QString>
#include <QDateTime>
#include <optional>
#include "domain/Types.h"

namespace domain::conversation {
    /**
     * @brief 会话实体（对应左侧边栏的单个对话）
     */
    struct Conversation {
        QUuid id; ///< 会话唯一全局标识符 (UUID)
        QString title; ///< 会话标题（支持自动生成或手动重命名）

        std::optional<QUuid> projectId; ///< 关联的项目 ID（普通全局对话为 std::nullopt，Agent 模式绑定具体项目）
        std::optional<QUuid> agentId; ///< 绑定的智能体 ID（普通对话为 std::nullopt）
        QString modelId; ///< 当前会话使用的模型名称 (如 "deepseek-v3")
        domain::ConversationMode mode; ///< 会话运行模式

        bool isPinned = false; ///< 是否置顶
        QDateTime createdAt; ///< 创建时间
        QDateTime updatedAt; ///< 最后更新时间
    };
} // namespace domain::conversation
