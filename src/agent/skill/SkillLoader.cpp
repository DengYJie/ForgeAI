#include "SkillLoader.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>

namespace agent::skill {

    std::optional<domain::agent::Skill> SkillLoader::loadMetadataFromFile(const QString& filePath) const {
        return loadFromFile(filePath, false);
    }

    std::optional<domain::agent::Skill> SkillLoader::loadFromFile(const QString& filePath) const {
        return loadFromFile(filePath, false);
    }

    std::optional<domain::agent::Skill> SkillLoader::loadFromFile(const QString& filePath, bool loadInstructionsImmediately) const {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentSkill, QStringLiteral("无法读取 Skill 文件"), {
                {QStringLiteral("path"), filePath},
                {QStringLiteral("error"), file.errorString()}
            });
            return std::nullopt;
        }

        const QString fullContent = QString::fromUtf8(file.readAll());
        file.close();

        domain::agent::Skill skill;
        skill.path = QFileInfo(filePath).canonicalFilePath();
        if (skill.path.isEmpty()) skill.path = filePath;
        skill.id = QFileInfo(filePath).dir().dirName();
        skill.name = skill.id;

        if (fullContent.startsWith(QStringLiteral("---"))) {
            int secondDash = fullContent.indexOf(QStringLiteral("---"), 3);
            if (secondDash != -1) {
                const QString header = fullContent.mid(3, secondDash - 3);
                if (loadInstructionsImmediately) {
                    skill.instructions = fullContent.mid(secondDash + 3).trimmed();
                    skill.instructionsLoaded = true;
                } else {
                    skill.instructionsLoaded = false;
                }

                const auto lines = header.split(QLatin1Char('\n'));
                for (const auto& line : lines) {
                    const QString trimmed = line.trimmed();
                    if (trimmed.startsWith(QStringLiteral("name:"))) {
                        skill.name = trimmed.mid(5).trimmed().remove(QLatin1Char('"')).remove(QLatin1Char('\''));
                    } else if (trimmed.startsWith(QStringLiteral("description:"))) {
                        skill.description = trimmed.mid(12).trimmed().remove(QLatin1Char('"')).remove(QLatin1Char('\''));
                    } else if (trimmed.startsWith(QStringLiteral("id:"))) {
                        skill.id = trimmed.mid(3).trimmed().remove(QLatin1Char('"')).remove(QLatin1Char('\''));
                    } else if (trimmed.startsWith(QStringLiteral("tags:"))) {
                        QString tagStr = trimmed.mid(5).trimmed().remove(QLatin1Char('[')).remove(QLatin1Char(']'));
                        const auto tags = tagStr.split(QLatin1Char(','));
                        for (const auto& t : tags) {
                            const QString cleanTag = t.trimmed().remove(QLatin1Char('"')).remove(QLatin1Char('\''));
                            if (!cleanTag.isEmpty()) skill.tags.append(cleanTag);
                        }
                    }
                }
                return skill;
            } else {
                core::logging::LoggingService::instance().warn(core::logging::Category::AgentSkill, QStringLiteral("Skill Frontmatter 格式无效，未找到闭合 ---"), {
                    {QStringLiteral("path"), filePath}
                });
            }
        }

        if (loadInstructionsImmediately) {
            skill.instructions = fullContent.trimmed();
            skill.instructionsLoaded = true;
        } else {
            skill.instructionsLoaded = false;
        }
        return skill;
    }

    bool SkillLoader::loadInstructions(domain::agent::Skill& skill) const {
        if (skill.instructionsLoaded && !skill.instructions.isEmpty()) {
            return true;
        }

        if (skill.path.isEmpty()) return false;

        QFile file(skill.path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            core::logging::LoggingService::instance().warn(core::logging::Category::AgentSkill, QStringLiteral("无法读取 Skill 指令正文"), {
                {QStringLiteral("skillId"), skill.id},
                {QStringLiteral("error"), file.errorString()}
            });
            return false;
        }

        const QString fullContent = QString::fromUtf8(file.readAll());
        file.close();

        if (fullContent.startsWith(QStringLiteral("---"))) {
            int secondDash = fullContent.indexOf(QStringLiteral("---"), 3);
            if (secondDash != -1) {
                skill.instructions = fullContent.mid(secondDash + 3).trimmed();
                skill.instructionsLoaded = true;
                return true;
            }
        }

        skill.instructions = fullContent.trimmed();
        skill.instructionsLoaded = true;
        return true;
    }

    QList<domain::agent::Skill> SkillLoader::scanDirectory(const QString& rootPath) const {
        return scanDirectory(rootPath, false);
    }

    QList<domain::agent::Skill> SkillLoader::scanDirectory(const QString& rootPath, bool loadInstructionsImmediately) const {
        QElapsedTimer timer;
        timer.start();

        QList<domain::agent::Skill> result;
        if (rootPath.isEmpty()) return result;

        const QStringList candidateDirs = {
            QDir(rootPath).filePath(QStringLiteral(".agents/skills")),
            QDir(rootPath).filePath(QStringLiteral(".skills"))
        };

        for (const auto& searchPath : candidateDirs) {
            QDir dir(searchPath);
            if (!dir.exists()) continue;

            const auto subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& subDirName : subDirs) {
                const QString skillFile = dir.filePath(subDirName + QStringLiteral("/SKILL.md"));
                if (QFile::exists(skillFile)) {
                    auto skillOpt = loadFromFile(skillFile, loadInstructionsImmediately);
                    if (skillOpt.has_value()) {
                        bool duplicate = false;
                        for (const auto& existing : result) {
                            if (existing.id == skillOpt->id || existing.path == skillOpt->path) {
                                duplicate = true;
                                break;
                            }
                        }
                        if (!duplicate) {
                            result.append(*skillOpt);
                        }
                    }
                }
            }
        }

        core::logging::LoggingService::instance().debug(core::logging::Category::AgentSkill, QStringLiteral("Skill 目录扫描完成"), {
            {QStringLiteral("rootPath"), rootPath},
            {QStringLiteral("discoveredCount"), QString::number(result.size())},
            {QStringLiteral("durationMs"), QString::number(timer.elapsed())}
        });

        return result;
    }

} // namespace agent::skill
