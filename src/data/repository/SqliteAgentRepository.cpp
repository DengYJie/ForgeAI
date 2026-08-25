#include "SqliteAgentRepository.h"
#include "data/sqlite/SqlHelper.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>

namespace data::repository {

    SqliteAgentRepository::SqliteAgentRepository(const QString& connectionName)
        : m_connectionName(connectionName.isEmpty() ? QStringLiteral("forgeai_db") : connectionName) {
        initializeDatabase();
    }

    bool SqliteAgentRepository::initializeDatabase() {
        const auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS agent ("
            "id TEXT PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "description TEXT, "
            "avatar TEXT, "
            "system_prompt TEXT, "
            "model_id TEXT, "
            "provider_id TEXT, "
            "enabled_tools TEXT, "
            "enabled_skills TEXT, "
            "project_id TEXT, "
            "created_at INTEGER, "
            "updated_at INTEGER"
            ")"
        );
        return sqlite::SqlHelper::exec(sql, db);
    }

    QList<domain::agent::Agent> SqliteAgentRepository::getAllAgents() const {
        QList<domain::agent::Agent> list;
        const auto db = QSqlDatabase::database(m_connectionName);
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("SELECT id, name, description, avatar, system_prompt, model_id, provider_id, enabled_tools, enabled_skills, project_id, created_at, updated_at FROM agent ORDER BY updated_at DESC"))) {
            return list;
        }

        while (q.next()) {
            domain::agent::Agent a;
            a.id = QUuid::fromString(q.value(0).toString());
            a.name = q.value(1).toString();
            a.description = q.value(2).toString();
            a.avatar = q.value(3).toString();
            a.systemPrompt = q.value(4).toString();
            a.modelId = q.value(5).toString();
            a.providerId = q.value(6).toString();

            const auto toolsDoc = QJsonDocument::fromJson(q.value(7).toString().toUtf8());
            for (const auto& val : toolsDoc.array()) {
                a.enabledTools.append(val.toString());
            }

            const auto skillsDoc = QJsonDocument::fromJson(q.value(8).toString().toUtf8());
            for (const auto& val : skillsDoc.array()) {
                a.enabledSkills.append(val.toString());
            }

            const QString projIdStr = q.value(9).toString();
            if (!projIdStr.isEmpty()) {
                a.projectId = QUuid::fromString(projIdStr);
            }

            a.createdAt = QDateTime::fromMSecsSinceEpoch(q.value(10).toLongLong());
            a.updatedAt = QDateTime::fromMSecsSinceEpoch(q.value(11).toLongLong());
            list.append(a);
        }

        return list;
    }

    std::optional<domain::agent::Agent> SqliteAgentRepository::getAgent(const QUuid& id) const {
        for (const auto& agent : getAllAgents()) {
            if (agent.id == id) {
                return agent;
            }
        }
        return std::nullopt;
    }

    bool SqliteAgentRepository::saveAgent(const domain::agent::Agent& agent) {
        const auto db = QSqlDatabase::database(m_connectionName);

        QJsonArray toolsArr;
        for (const auto& t : agent.enabledTools) toolsArr.append(t);
        const QString toolsJson = QString::fromUtf8(QJsonDocument(toolsArr).toJson(QJsonDocument::Compact));

        QJsonArray skillsArr;
        for (const auto& s : agent.enabledSkills) skillsArr.append(s);
        const QString skillsJson = QString::fromUtf8(QJsonDocument(skillsArr).toJson(QJsonDocument::Compact));

        const QString sql = QStringLiteral(
            "INSERT OR REPLACE INTO agent ("
            "id, name, description, avatar, system_prompt, model_id, provider_id, enabled_tools, enabled_skills, project_id, created_at, updated_at"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );

        const QVariantList args = {
            agent.id.toString(),
            agent.name,
            agent.description,
            agent.avatar,
            agent.systemPrompt,
            agent.modelId,
            agent.providerId,
            toolsJson,
            skillsJson,
            agent.projectId.has_value() ? agent.projectId->toString() : QString(),
            agent.createdAt.toMSecsSinceEpoch(),
            agent.updatedAt.toMSecsSinceEpoch()
        };

        return sqlite::SqlHelper::exec(sql, args, db);
    }

    bool SqliteAgentRepository::deleteAgent(const QUuid& id) {
        const auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral("DELETE FROM agent WHERE id = ?");
        return sqlite::SqlHelper::exec(sql, {id.toString()}, db);
    }

} // namespace data::repository
