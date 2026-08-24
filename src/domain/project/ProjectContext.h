#pragma once
#include <QString>
#include <QList>

namespace domain::project {
struct ProjectSkill {
    QString name;
    QString path;
    QString instructions;
    bool operator==(const ProjectSkill&) const = default;
};
struct ProjectContext {
    QString rootPath;
    QString agentsInstructions;
    QList<ProjectSkill> skills;
    QString mcpConfigPath;
    QString mcpConfigContent;
    bool operator==(const ProjectContext&) const = default;
};
} // namespace domain::project
