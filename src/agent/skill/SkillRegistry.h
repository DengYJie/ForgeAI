#pragma once

#include <QString>
#include <QList>
#include <QHash>
#include <mutex>
#include <optional>
#include "domain/agent/Skill.h"

namespace agent::skill {

    /**
     * @brief Skill 注册管理中心（支持按 ID/Name 注册、查找与启禁用过滤）
     */
    class SkillRegistry {
    public:
        SkillRegistry() = default;
        ~SkillRegistry() = default;

        bool registerSkill(const domain::agent::Skill& skill);
        void registerSkills(const QList<domain::agent::Skill>& skills);
        void unregisterSkill(const QString& idOrName);
        void clear();

        std::optional<domain::agent::Skill> findSkill(const QString& idOrName) const;
        bool hasSkill(const QString& idOrName) const;

        QList<domain::agent::Skill> allSkills() const;
        QList<domain::agent::Skill> enabledSkills() const;

        void setSkillEnabled(const QString& idOrName, bool enabled);

    private:
        mutable std::mutex m_mutex;
        QHash<QString, domain::agent::Skill> m_skills;
    };

} // namespace agent::skill
