#include "SqliteConversationRepository.h"
#include "data/sqlite/SqlHelper.h"
#include <QDateTime>
#include <QDebug>

namespace data::repository {

    namespace {
        domain::conversation::Conversation mapConversation(const QSqlQuery &query) {
            domain::conversation::Conversation conv;
            conv.id = QUuid::fromString(query.value(0).toString());
            const QString projStr = query.value(1).toString();
            if (!projStr.isEmpty()) conv.projectId = QUuid::fromString(projStr);
            const QString agentStr = query.value(2).toString();
            if (!agentStr.isEmpty()) conv.agentId = QUuid::fromString(agentStr);
            conv.title = query.value(3).toString();
            conv.isPinned = query.value(4).toBool();
            conv.isArchived = query.value(5).toBool();
            conv.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong());
            conv.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(7).toLongLong());
            return conv;
        }
    } // anonymous namespace

    SqliteConversationRepository::SqliteConversationRepository(const QString &connectionName)
        : m_connectionName(connectionName) {
    }

    SqliteConversationRepository::~SqliteConversationRepository() = default;

    bool SqliteConversationRepository::initializeDatabase() {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString createConversationTable = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS conversation ("
            "    id TEXT PRIMARY KEY,"
            "    project_id TEXT,"
            "    agent_id TEXT,"
            "    title TEXT NOT NULL,"
            "    is_pinned INTEGER DEFAULT 0,"
            "    is_archived INTEGER DEFAULT 0,"
            "    created_at INTEGER NOT NULL,"
            "    updated_at INTEGER NOT NULL"
            ");"
        );
        return data::sqlite::SqlHelper::exec(createConversationTable, db);
    }

    QList<domain::conversation::Conversation> SqliteConversationRepository::getAllConversations() {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "SELECT id, project_id, agent_id, title, is_pinned, is_archived, created_at, updated_at "
            "FROM conversation ORDER BY updated_at DESC;"
        );
        return data::sqlite::SqlHelper::queryAll<domain::conversation::Conversation>(sql, db, mapConversation);
    }

    std::optional<domain::conversation::Conversation> SqliteConversationRepository::getConversation(const QUuid &id) {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "SELECT id, project_id, agent_id, title, is_pinned, is_archived, created_at, updated_at "
            "FROM conversation WHERE id = ?;"
        );
        return data::sqlite::SqlHelper::queryOne<domain::conversation::Conversation>(sql, {id.toString()}, db, mapConversation);
    }

    void SqliteConversationRepository::saveConversation(const domain::conversation::Conversation &conversation) {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "INSERT OR REPLACE INTO conversation (id, project_id, agent_id, title, is_pinned, is_archived, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);"
        );
        const QVariantList args = {
            conversation.id.toString(),
            conversation.projectId.has_value() ? conversation.projectId->toString() : QString(),
            conversation.agentId.has_value() ? conversation.agentId->toString() : QString(),
            conversation.title,
            conversation.isPinned ? 1 : 0,
            conversation.isArchived ? 1 : 0,
            conversation.createdAt.toMSecsSinceEpoch(),
            conversation.updatedAt.toMSecsSinceEpoch()
        };
        const auto result = data::sqlite::SqlHelper::execute(sql, args, db);
        if (!result) {
            qWarning().noquote() << QStringLiteral("[SqliteConversationRepository] saveConversation failed: %1")
                .arg(result.error.message);
        }
    }

    void SqliteConversationRepository::deleteConversation(const QUuid &id) {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral("DELETE FROM conversation WHERE id = ?;");
        const auto result = data::sqlite::SqlHelper::execute(sql, {id.toString()}, db);
        if (!result) {
            qWarning().noquote() << QStringLiteral("[SqliteConversationRepository] deleteConversation failed: %1")
                .arg(result.error.message);
        }
    }

} // namespace data::repository
