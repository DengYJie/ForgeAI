#pragma once

#include <QString>
#include <QList>
#include <optional>
#include "domain/agent/Skill.h"

namespace agent::skill {

    /**
     * @brief SKILL.md 文件与 Frontmatter 解析加载器
     */
    class SkillLoader {
    public:
        SkillLoader() = default;
        ~SkillLoader() = default;

        /**
         * @brief 仅解析 SKILL.md 的 Frontmatter 元数据（延迟加载指令内容）
         */
        std::optional<domain::agent::Skill> loadMetadataFromFile(const QString& filePath) const;

        /**
         * @brief 加载指定文件的全部内容（含元数据与指令体）
         */
        std::optional<domain::agent::Skill> loadFromFile(const QString& filePath) const;
        std::optional<domain::agent::Skill> loadFromFile(const QString& filePath, bool loadInstructionsImmediately) const;

        /**
         * @brief 为指定 Skill 实体按需延迟加载完整 Instructions 内容
         */
        bool loadInstructions(domain::agent::Skill& skill) const;

        /**
         * @brief 扫描指定目录（如工作区根目录）下所有 .agents/skills/ 与 .skills/ 子目录中的 SKILL.md
         */
        QList<domain::agent::Skill> scanDirectory(const QString& rootPath) const;
        QList<domain::agent::Skill> scanDirectory(const QString& rootPath, bool loadInstructionsImmediately) const;
    };

} // namespace agent::skill
