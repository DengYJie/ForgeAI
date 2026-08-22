#pragma once
#include <QUuid>
#include <QString>
#include <QStringList>
#include <QDateTime>

namespace domain::agent {
    /**
     * @brief 智能体配置实体（定义 Agent 的人设、绑定模型与工具列表）
     */
    struct Agent {
        QUuid id; ///< 智能体全局唯一标识符
        QString name; ///< 智能体名称
        QString description; ///< 智能体功能简述
        QString avatar; ///< 头像资源路径或图标名称

        QString systemPrompt; ///< 核心系统提示词（人设约束）
        QString modelId; ///< 默认绑定的模型名称

        QStringList enabledTools; ///< 已启用的工具/MCP 插件名称列表

        QDateTime createdAt; ///< 创建时间
    };
} // namespace domain::agent
