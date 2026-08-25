#pragma once
#include <QUuid>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <optional>

namespace domain::project {
    /**
     * @brief 项目/工作区实体（为 Agent 提供物理根目录与环境锚定）
     */
    struct Project {
        QUuid id; ///< 项目全局唯一标识符
        QString name; ///< 项目展示名称 (如 "ForgeAI")
        QString rootPath; ///< 项目在本地磁盘的绝对路径 (如 "F:/source/ForgeAI")

        QString customRules; ///< 项目专属规则（对应项目根目录下的 .forgeairules 或用户配置）
        std::optional<QUuid> defaultAgentId; ///< 该项目默认使用的智能体 (可选)
        QString defaultModelId; ///< 该项目默认使用的模型名称

        QStringList ignorePatterns; ///< 文件扫描与工具操作忽略规则 (如 [".git", "build", "node_modules"])

        QDateTime createdAt; ///< 项目添加时间
        QDateTime lastOpenedAt; ///< 最近一次打开时间
        bool isPinned = false; ///< Whether the project is pinned in the sidebar

        bool operator==(const Project&) const = default;
    };
} // namespace domain::project
