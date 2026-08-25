#pragma once

#include <QString>
#include <QList>
#include <optional>
#include "domain/agent/Skill.h"

namespace agent::skill {

    /**
     * @brief Skill 解析加载器（负责从 SKILL.md 或目录中解析 Frontmatter 与 Instructions）
     */
    class SkillLoader {
    public:
        SkillLoader() = default;
        ~SkillLoader() = default;

        /**
         * @brief 从 SKILL.md 文件中解析并加载 Skill
         */
        std::optional<domain::agent::Skill> loadFromFile(const QString& filePath) const;

        /**
         * @brief 从包含 SKILL.md 的目录中加载 Skill
         */
        std::optional<domain::agent::Skill> loadFromDirectory(const QString& dirPath) const;

        /**
         * @brief 扫描目标根目录（如 .agents/skills 或 .skills）下的全部子目录加载 Skills
         */
        QList<domain::agent::Skill> scanDirectory(const QString& baseSkillsDirPath) const;
    };

} // namespace agent::skill
