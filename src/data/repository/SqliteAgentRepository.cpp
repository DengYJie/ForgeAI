#include "SqliteAgentRepository.h"
#include "data/sqlite/SqlHelper.h"

#include <QSqlDatabase>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

namespace data::repository {

    namespace {
        domain::agent::Agent mapAgent(const QSqlQuery& q) {
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

            const auto mcpDoc = QJsonDocument::fromJson(q.value(9).toString().toUtf8());
            for (const auto& val : mcpDoc.array()) {
                a.enabledMcpServerIds.append(val.toString());
            }

            const QString projIdStr = q.value(10).toString();
            if (!projIdStr.isEmpty()) {
                a.projectId = QUuid::fromString(projIdStr);
            }

            a.createdAt = QDateTime::fromMSecsSinceEpoch(q.value(11).toLongLong());
            a.updatedAt = QDateTime::fromMSecsSinceEpoch(q.value(12).toLongLong());
            return a;
        }
    } // anonymous namespace

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
            "enabled_mcp_servers TEXT, "
            "project_id TEXT, "
            "created_at INTEGER, "
            "updated_at INTEGER"
            ")"
        );
        return sqlite::SqlHelper::exec(sql, db);
    }

    QList<domain::agent::Agent> SqliteAgentRepository::getAllAgents() const {
        const auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "SELECT id, name, description, avatar, system_prompt, model_id, provider_id, "
            "enabled_tools, enabled_skills, enabled_mcp_servers, project_id, created_at, updated_at "
            "FROM agent ORDER BY updated_at DESC;"
        );
        return data::sqlite::SqlHelper::queryAll<domain::agent::Agent>(sql, db, mapAgent);
    }

    std::optional<domain::agent::Agent> SqliteAgentRepository::getAgent(const QUuid& id) const {
        const auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "SELECT id, name, description, avatar, system_prompt, model_id, provider_id, "
            "enabled_tools, enabled_skills, enabled_mcp_servers, project_id, created_at, updated_at "
            "FROM agent WHERE id = ?;"
        );
        return data::sqlite::SqlHelper::queryOne<domain::agent::Agent>(sql, {id.toString()}, db, mapAgent);
    }

    bool SqliteAgentRepository::saveAgent(const domain::agent::Agent& agent) {
        const auto db = QSqlDatabase::database(m_connectionName);

        QJsonArray toolsArr;
        for (const auto& t : agent.enabledTools) toolsArr.append(t);
        const QString toolsJson = QString::fromUtf8(QJsonDocument(toolsArr).toJson(QJsonDocument::Compact));

        QJsonArray skillsArr;
        for (const auto& s : agent.enabledSkills) skillsArr.append(s);
        const QString skillsJson = QString::fromUtf8(QJsonDocument(skillsArr).toJson(QJsonDocument::Compact));

        QJsonArray mcpArr;
        for (const auto& m : agent.enabledMcpServerIds) mcpArr.append(m);
        const QString mcpJson = QString::fromUtf8(QJsonDocument(mcpArr).toJson(QJsonDocument::Compact));

        const QString sql = QStringLiteral(
            "INSERT OR REPLACE INTO agent ("
            "id, name, description, avatar, system_prompt, model_id, provider_id, enabled_tools, enabled_skills, enabled_mcp_servers, project_id, created_at, updated_at"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
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
            mcpJson,
            agent.projectId.has_value() ? agent.projectId->toString() : QString(),
            agent.createdAt.toMSecsSinceEpoch(),
            agent.updatedAt.toMSecsSinceEpoch()
        };

        const auto result = sqlite::SqlHelper::execute(sql, args, db);
        if (!result) {
            qWarning().noquote() << QStringLiteral("[SqliteAgentRepository] saveAgent failed: %1")
                .arg(result.error.message);
        }
        return result.success;
    }

    bool SqliteAgentRepository::deleteAgent(const QUuid& id) {
        const auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral("DELETE FROM agent WHERE id = ?;");
        const auto result = sqlite::SqlHelper::execute(sql, {id.toString()}, db);
        if (!result) {
            qWarning().noquote() << QStringLiteral("[SqliteAgentRepository] deleteAgent failed: %1")
                .arg(result.error.message);
        }
        return result.success;
    }

} // namespace data::repository
