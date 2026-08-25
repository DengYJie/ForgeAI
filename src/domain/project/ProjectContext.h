#pragma once

#include <QString>
#include <QList>
#include "domain/agent/Skill.h"

namespace domain::project {

    /**
     * @brief 项目上下文结构
     */
    struct ProjectContext {
        QString rootPath;
        QString agentsInstructions;
        QList<domain::agent::Skill> skills;
        QString mcpConfigPath;
        QString mcpConfigContent;

        bool operator==(const ProjectContext &other) const = default;
    };

} // namespace domain::project
