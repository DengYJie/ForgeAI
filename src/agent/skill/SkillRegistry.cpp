#include "SkillRegistry.h"

namespace agent::skill {

    bool SkillRegistry::registerSkill(const domain::agent::Skill& skill) {
        if (skill.id.trimmed().isEmpty() && skill.name.trimmed().isEmpty()) return false;

        const QString key = !skill.id.isEmpty() ? skill.id : skill.name;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_skills.insert(key, skill);
        return true;
    }

    void SkillRegistry::registerSkills(const QList<domain::agent::Skill>& skills) {
        for (const auto& skill : skills) {
            registerSkill(skill);
        }
    }

    void SkillRegistry::unregisterSkill(const QString& idOrName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_skills.remove(idOrName);
    }

    void SkillRegistry::clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_skills.clear();
    }

    std::optional<domain::agent::Skill> SkillRegistry::findSkill(const QString& idOrName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_skills.contains(idOrName)) {
            return m_skills.value(idOrName);
        }
        for (const auto& s : m_skills) {
            if (s.name == idOrName) {
                return s;
            }
        }
        return std::nullopt;
    }

    bool SkillRegistry::hasSkill(const QString& idOrName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_skills.contains(idOrName)) return true;
        for (const auto& s : m_skills) {
            if (s.name == idOrName) return true;
        }
        return false;
    }

    QList<domain::agent::Skill> SkillRegistry::allSkills() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_skills.values();
    }

    QList<domain::agent::Skill> SkillRegistry::enabledSkills() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<domain::agent::Skill> result;
        for (const auto& s : m_skills) {
            if (s.isEnabled) {
                result.append(s);
            }
        }
        return result;
    }

    void SkillRegistry::setSkillEnabled(const QString& idOrName, bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_skills.contains(idOrName)) {
            m_skills[idOrName].isEnabled = enabled;
            return;
        }
        for (auto it = m_skills.begin(); it != m_skills.end(); ++it) {
            if (it->name == idOrName) {
                it->isEnabled = enabled;
                return;
            }
        }
    }

} // namespace agent::skill
