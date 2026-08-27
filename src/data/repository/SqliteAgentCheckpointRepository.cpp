#include "SqliteAgentCheckpointRepository.h"
#include "data/sqlite/SqlHelper.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>

namespace data::repository {

    SqliteAgentCheckpointRepository::SqliteAgentCheckpointRepository(const QString& connectionName)
        : m_connectionName(connectionName.isEmpty() ? QStringLiteral("forgeai_db") : connectionName) {
        initializeDatabase();
    }

    bool SqliteAgentCheckpointRepository::initializeDatabase() {
        const auto db = QSqlDatabase::database(m_connectionName);
        const QString createTableSql = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS agent_checkpoint ("
            "id TEXT PRIMARY KEY, "
            "session_id TEXT NOT NULL, "
            "run_id TEXT NOT NULL, "
            "round_index INTEGER NOT NULL, "
            "status INTEGER NOT NULL, "
            "pending_tool_calls TEXT, "
            "pending_tool_results TEXT, "
            "timestamp INTEGER NOT NULL"
            ")"
        );
        bool ok = sqlite::SqlHelper::exec(createTableSql, db);
        if (ok) {
            const QString createIndexSql = QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_checkpoint_session ON agent_checkpoint(session_id, timestamp DESC)"
            );
            sqlite::SqlHelper::exec(createIndexSql, db);
        }
        return ok;
    }

    bool SqliteAgentCheckpointRepository::saveCheckpoint(const domain::agent::AgentCheckpoint& checkpoint) {
        const auto db = QSqlDatabase::database(m_connectionName);

        QJsonArray callsArr;
        for (const auto& call : checkpoint.pendingToolCalls) {
            callsArr.append(QJsonObject{
                {QStringLiteral("id"), call.id},
                {QStringLiteral("name"), call.name},
                {QStringLiteral("arguments"), call.arguments},
                {QStringLiteral("protocolMetadata"), call.protocolMetadata}
            });
        }

        QJsonArray resultsArr;
        for (const auto& res : checkpoint.pendingToolResults) {
            resultsArr.append(QJsonObject{
                {QStringLiteral("toolCallId"), res.toolCallId},
                {QStringLiteral("content"), res.content},
                {QStringLiteral("isError"), res.isError}
            });
        }

        const QString sql = QStringLiteral(
            "INSERT OR REPLACE INTO agent_checkpoint ("
            "id, session_id, run_id, round_index, status, pending_tool_calls, pending_tool_results, timestamp"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
        );

        const QVariantList args = {
            checkpoint.checkpointId.toString(),
            checkpoint.sessionId,
            checkpoint.runId.toString(),
            checkpoint.roundIndex,
            static_cast<int>(checkpoint.status),
            QString::fromUtf8(QJsonDocument(callsArr).toJson(QJsonDocument::Compact)),
            QString::fromUtf8(QJsonDocument(resultsArr).toJson(QJsonDocument::Compact)),
            checkpoint.timestamp.toMSecsSinceEpoch()
        };

        return sqlite::SqlHelper::exec(sql, args, db);
    }

    std::optional<domain::agent::AgentCheckpoint> SqliteAgentCheckpointRepository::getLatestCheckpoint(const QString& sessionId) const {
        const auto db = QSqlDatabase::database(m_connectionName);
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT id, session_id, run_id, round_index, status, pending_tool_calls, pending_tool_results, timestamp "
            "FROM agent_checkpoint WHERE session_id = ? ORDER BY timestamp DESC LIMIT 1"
        ));
        q.addBindValue(sessionId);

        if (q.exec() && q.next()) {
            domain::agent::AgentCheckpoint cp;
            cp.checkpointId = QUuid::fromString(q.value(0).toString());
            cp.sessionId = q.value(1).toString();
            cp.runId = QUuid::fromString(q.value(2).toString());
            cp.roundIndex = q.value(3).toInt();
            cp.status = static_cast<domain::agent::AgentRunStatus>(q.value(4).toInt());

            const auto callsDoc = QJsonDocument::fromJson(q.value(5).toString().toUtf8());
            for (const auto& val : callsDoc.array()) {
                const auto obj = val.toObject();
                cp.pendingToolCalls.append(domain::agent::ToolCall{
                    obj.value(QStringLiteral("id")).toString(),
                    obj.value(QStringLiteral("name")).toString(),
                    obj.value(QStringLiteral("arguments")).toString(),
                    obj.value(QStringLiteral("protocolMetadata")).toObject()
                });
            }

            const auto resultsDoc = QJsonDocument::fromJson(q.value(6).toString().toUtf8());
            for (const auto& val : resultsDoc.array()) {
                const auto obj = val.toObject();
                cp.pendingToolResults.append(domain::agent::ToolResult{
                    obj.value(QStringLiteral("toolCallId")).toString(),
                    obj.value(QStringLiteral("content")).toString(),
                    obj.value(QStringLiteral("isError")).toBool()
                });
            }

            cp.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(7).toLongLong());
            return cp;
        }

        return std::nullopt;
    }

    bool SqliteAgentCheckpointRepository::deleteCheckpointsForSession(const QString& sessionId) {
        const auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral("DELETE FROM agent_checkpoint WHERE session_id = ?");
        return sqlite::SqlHelper::exec(sql, {sessionId}, db);
    }

} // namespace data::repository
