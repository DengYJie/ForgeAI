#include "SkillLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>

namespace agent::skill {

    std::optional<domain::agent::Skill> SkillLoader::loadFromFile(const QString& filePath) const {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return std::nullopt;
        }

        const QString rawContent = QString::fromUtf8(file.readAll());
        file.close();

        domain::agent::Skill skill;
        skill.path = filePath;
        const QString dirName = QFileInfo(filePath).dir().dirName();
        skill.name = dirName;
        skill.id = dirName;

        // 解析 YAML Frontmatter (若存在)
        if (rawContent.startsWith(QStringLiteral("---"))) {
            const int secondMarker = rawContent.indexOf(QStringLiteral("---"), 3);
            if (secondMarker != -1) {
                const QString frontmatter = rawContent.mid(3, secondMarker - 3);
                skill.instructions = rawContent.mid(secondMarker + 3).trimmed();

                const auto lines = frontmatter.split(QLatin1Char('\n'));
                for (const auto& line : lines) {
                    const QString trimmed = line.trimmed();
                    if (trimmed.startsWith(QStringLiteral("name:"))) {
                        skill.name = trimmed.mid(5).trimmed();
                        if (skill.name.startsWith(QLatin1Char('"')) && skill.name.endsWith(QLatin1Char('"'))) {
                            skill.name = skill.name.mid(1, skill.name.length() - 2);
                        }
                    } else if (trimmed.startsWith(QStringLiteral("description:"))) {
                        skill.description = trimmed.mid(12).trimmed();
                        if (skill.description.startsWith(QLatin1Char('"')) && skill.description.endsWith(QLatin1Char('"'))) {
                            skill.description = skill.description.mid(1, skill.description.length() - 2);
                        }
                    } else if (trimmed.startsWith(QStringLiteral("id:"))) {
                        skill.id = trimmed.mid(3).trimmed();
                    }
                }
            } else {
                skill.instructions = rawContent.trimmed();
            }
        } else {
            skill.instructions = rawContent.trimmed();
        }

        if (skill.name.isEmpty()) {
            skill.name = dirName;
        }
        if (skill.id.isEmpty()) {
            skill.id = skill.name;
        }

        return skill;
    }

    std::optional<domain::agent::Skill> SkillLoader::loadFromDirectory(const QString& dirPath) const {
        const QDir dir(dirPath);
        const QString skillFile = dir.filePath(QStringLiteral("SKILL.md"));
        if (QFile::exists(skillFile)) {
            return loadFromFile(skillFile);
        }
        return std::nullopt;
    }

    QList<domain::agent::Skill> SkillLoader::scanDirectory(const QString& baseSkillsDirPath) const {
        QList<domain::agent::Skill> result;
        const QDir dir(baseSkillsDirPath);
        if (!dir.exists()) return result;

        const auto subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& subDir : subDirs) {
            const QString subDirPath = dir.filePath(subDir);
            auto skillOpt = loadFromDirectory(subDirPath);
            if (skillOpt.has_value()) {
                result.append(skillOpt.value());
            }
        }
        return result;
    }

} // namespace agent::skill
