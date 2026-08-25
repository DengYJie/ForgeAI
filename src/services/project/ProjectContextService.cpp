#include "ProjectContextService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "agent/skill/SkillLoader.h"

namespace services::project {

namespace {
QString readText(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll()) : QString();
}
}

domain::project::ProjectContext ProjectContextService::load(const QString& rootPath) const {
    domain::project::ProjectContext context;
    context.rootPath = QDir(rootPath).canonicalPath();
    if (context.rootPath.isEmpty()) return context;

    const QDir root(context.rootPath);
    context.agentsInstructions = readText(root.filePath(QStringLiteral("AGENTS.md")));

    agent::skill::SkillLoader skillLoader;
    context.skills = skillLoader.scanDirectory(context.rootPath, false);

    const QString mcpJson = root.filePath(QStringLiteral(".mcp.json"));
    const QString mcpJsonc = root.filePath(QStringLiteral("mcp.json"));
    if (QFileInfo::exists(mcpJson)) {
        context.mcpConfigPath = mcpJson;
    } else if (QFileInfo::exists(mcpJsonc)) {
        context.mcpConfigPath = mcpJsonc;
    }

    if (!context.mcpConfigPath.isEmpty()) {
        context.mcpConfigContent = readText(context.mcpConfigPath);
    }

    return context;
}

} // namespace services::project
