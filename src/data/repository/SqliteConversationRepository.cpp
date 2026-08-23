#include "SqliteConversationRepository.h"
#include "data/sqlite/SqlHelper.h"
#include "data/sqlite/SqlTransaction.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>

namespace data::repository {
    SqliteConversationRepository::SqliteConversationRepository(const QString &connectionName)
        : m_connectionName(connectionName) {
    }

    SqliteConversationRepository::~SqliteConversationRepository() = default;

    bool SqliteConversationRepository::initializeDatabase() {
        auto db = QSqlDatabase::database(m_connectionName);
        if (!db.isOpen()) {
            return false;
        }

        const QString createConversationTable = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS conversation ("
            "    id TEXT PRIMARY KEY,"
            "    project_id TEXT,"
            "    agent_id TEXT,"
            "    title TEXT NOT NULL,"
            "    is_pinned INTEGER DEFAULT 0,"
            "    created_at INTEGER NOT NULL,"
            "    updated_at INTEGER NOT NULL"
            ");"
        );

        return data::sqlite::SqlHelper::exec(createConversationTable, db);
    }

    QList<domain::conversation::Conversation> SqliteConversationRepository::getAllConversations() {
        QList<domain::conversation::Conversation> result;
        auto db = QSqlDatabase::database(m_connectionName);
        if (!db.isOpen()) return result;

        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("SELECT id, project_id, agent_id, title, is_pinned, created_at, updated_at FROM conversation ORDER BY updated_at DESC;"))) {
            return result;
        }

        while (query.next()) {
            domain::conversation::Conversation conv;
            conv.id = QUuid::fromString(query.value(0).toString());
            const QString projStr = query.value(1).toString();
            if (!projStr.isEmpty()) conv.projectId = QUuid::fromString(projStr);
            const QString agentStr = query.value(2).toString();
            if (!agentStr.isEmpty()) conv.agentId = QUuid::fromString(agentStr);
            conv.title = query.value(3).toString();
            conv.isPinned = query.value(4).toBool();
            conv.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(5).toLongLong());
            conv.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong());
            result.append(conv);
        }
        return result;
    }

    std::optional<domain::conversation::Conversation> SqliteConversationRepository::getConversation(const QUuid &id) {
        auto db = QSqlDatabase::database(m_connectionName);
        if (!db.isOpen()) return std::nullopt;

        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT id, project_id, agent_id, title, is_pinned, created_at, updated_at FROM conversation WHERE id = ?;"));
        query.addBindValue(id.toString());
        if (!query.exec() || !query.next()) {
            return std::nullopt;
        }

        domain::conversation::Conversation conv;
        conv.id = QUuid::fromString(query.value(0).toString());
        const QString projStr = query.value(1).toString();
        if (!projStr.isEmpty()) conv.projectId = QUuid::fromString(projStr);
        const QString agentStr = query.value(2).toString();
        if (!agentStr.isEmpty()) conv.agentId = QUuid::fromString(agentStr);
        conv.title = query.value(3).toString();
        conv.isPinned = query.value(4).toBool();
        conv.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(5).toLongLong());
        conv.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong());
        return conv;
    }

    void SqliteConversationRepository::saveConversation(const domain::conversation::Conversation &conversation) {
        auto db = QSqlDatabase::database(m_connectionName);
        if (!db.isOpen()) return;

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO conversation (id, project_id, agent_id, title, is_pinned, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);"
        ));
        query.addBindValue(conversation.id.toString());
        query.addBindValue(conversation.projectId.has_value() ? conversation.projectId->toString() : QString());
        query.addBindValue(conversation.agentId.has_value() ? conversation.agentId->toString() : QString());
        query.addBindValue(conversation.title);
        query.addBindValue(conversation.isPinned ? 1 : 0);
        query.addBindValue(conversation.createdAt.toMSecsSinceEpoch());
        query.addBindValue(conversation.updatedAt.toMSecsSinceEpoch());
        query.exec();
    }

    void SqliteConversationRepository::deleteConversation(const QUuid &id) {
        auto db = QSqlDatabase::database(m_connectionName);
        if (!db.isOpen()) return;

        QSqlQuery query(db);
        query.prepare(QStringLiteral("DELETE FROM conversation WHERE id = ?;"));
        query.addBindValue(id.toString());
        query.exec();
    }
} // namespace data::repository
