#pragma once

#include <QString>
#include <QStringList>

namespace domain::agent {

    /**
     * @brief Agent 技能实体模型
     */
    struct Skill {
        QString id;
        QString name;
        QString description;
        QString instructions;
        QString path;
        QStringList tools;
        bool isEnabled = true;

        bool operator==(const Skill &other) const = default;
    };

} // namespace domain::agent
