#pragma once

#include <QString>
#include <QStringList>

namespace domain::agent {

    /**
     * @brief Agent 技能实体模型
     */
    struct Skill {
        QString id;                     ///< 技能唯一标识
        QString name;                   ///< 技能展示名称
        QString description;            ///< 技能简述
        QString path;                   ///< SKILL.md 文件绝对路径
        QString instructions;           ///< 技能指令内容（支持延迟加载）
        QStringList tags;               ///< 技能标签分类
        QStringList tools;              ///< 技能关联的工具列表
        bool isEnabled = true;          ///< 是否启用
        bool instructionsLoaded = false;///< 指令内容是否已加载完成

        bool operator==(const Skill &other) const = default;
    };

} // namespace domain::agent
