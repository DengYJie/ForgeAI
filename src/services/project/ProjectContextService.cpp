#include "ProjectContextService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace services::project {
namespace { QString readText(const QString& path) { QFile file(path); return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll()) : QString(); } }
domain::project::ProjectContext ProjectContextService::load(const QString& rootPath) const {
    domain::project::ProjectContext context;
    context.rootPath = QDir(rootPath).canonicalPath();
    if (context.rootPath.isEmpty()) return context;
    const QDir root(context.rootPath);
    context.agentsInstructions = readText(root.filePath(QStringLiteral("AGENTS.md")));
    const QDir skillsDir(root.filePath(QStringLiteral(".agents/skills")));
    for (const QFileInfo& entry : skillsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString skillPath = QDir(entry.absoluteFilePath()).filePath(QStringLiteral("SKILL.md"));
        const QString instructions = readText(skillPath);
        if (!instructions.isEmpty()) context.skills.append({entry.fileName(), skillPath, instructions});
    }
    const QString mcpJson = root.filePath(QStringLiteral(".mcp.json"));
    const QString mcpJsonc = root.filePath(QStringLiteral("mcp.json"));
    if (QFileInfo::exists(mcpJson)) context.mcpConfigPath = mcpJson;
    else if (QFileInfo::exists(mcpJsonc)) context.mcpConfigPath = mcpJsonc;
    if (!context.mcpConfigPath.isEmpty()) {
        // MCP transport startup is deliberately a separate capability.  The
        // agent still receives the project-declared server context now.
        context.mcpConfigContent = readText(context.mcpConfigPath);
    }
    return context;
}
} // namespace services::project
