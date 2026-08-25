#pragma once

#include <QUuid>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <optional>

namespace domain::agent {
    /**
     * @brief 智能体配置实体（定义 Agent 的人设、绑定模型与技能/工具列表）
     */
    struct Agent {
        QUuid id;                     ///< 智能体全局唯一标识符
        QString name;                 ///< 智能体名称
        QString description;          ///< 智能体功能简述
        QString avatar;               ///< 头像资源路径或图标名称

        QString systemPrompt;         ///< 核心系统提示词（人设约束）
        QString modelId;              ///< 默认绑定的模型名称
        QString providerId;           ///< 默认绑定的提供商 ID

        QStringList enabledTools;     ///< 已启用的工具列表
        QStringList enabledSkills;    ///< 已启用的 Skill 标识符列表

        std::optional<QUuid> projectId; ///< 关联的项目 ID（可选）

        QDateTime createdAt;          ///< 创建时间
        QDateTime updatedAt;          ///< 更新时间

        bool operator==(const Agent &other) const = default;
    };
} // namespace domain::agent
