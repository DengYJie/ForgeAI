#include "SqliteProjectRepository.h"
#include "data/sqlite/SqlHelper.h"
#include <QDateTime>
#include <QDebug>

namespace data::repository {

    namespace {
        domain::project::Project mapProject(const QSqlQuery &query) {
            domain::project::Project p;
            p.id = QUuid(query.value(0).toString());
            p.name = query.value(1).toString();
            p.rootPath = query.value(2).toString();
            p.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong());
            p.lastOpenedAt = QDateTime::fromMSecsSinceEpoch(query.value(4).toLongLong());
            p.isPinned = query.value(5).toInt() != 0;
            return p;
        }
    } // anonymous namespace

    SqliteProjectRepository::SqliteProjectRepository(const QString &connectionName)
        : m_connectionName(connectionName) {
    }

    bool SqliteProjectRepository::initializeDatabase() {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString createTableSql = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS project ("
            "id TEXT PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "root_path TEXT UNIQUE NOT NULL, "
            "created_at INTEGER, "
            "last_opened_at INTEGER, "
            "is_pinned INTEGER DEFAULT 0"
            ");"
        );
        const auto res = data::sqlite::SqlHelper::execute(createTableSql, db);
        if (!res) {
            qWarning().noquote() << QStringLiteral("[SqliteProjectRepository] Failed to create project table: %1")
                .arg(res.error.message);
            return false;
        }

        // Migration: add is_pinned column to existing project table if missing
        if (!data::sqlite::SqlHelper::columnExists(QStringLiteral("project"), QStringLiteral("is_pinned"), db)) {
            const auto altRes = data::sqlite::SqlHelper::execute(
                QStringLiteral("ALTER TABLE project ADD COLUMN is_pinned INTEGER DEFAULT 0;"),
                db
            );
            if (!altRes) {
                qWarning().noquote() << QStringLiteral("[SqliteProjectRepository] Failed to add is_pinned column: %1")
                    .arg(altRes.error.message);
            }
        }
        return true;
    }

    QList<domain::project::Project> SqliteProjectRepository::getAllProjects() {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "SELECT id, name, root_path, created_at, last_opened_at, is_pinned "
            "FROM project ORDER BY last_opened_at DESC;"
        );
        return data::sqlite::SqlHelper::queryAll<domain::project::Project>(sql, db, mapProject);
    }

    std::optional<domain::project::Project> SqliteProjectRepository::getProject(const QUuid &id) {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "SELECT id, name, root_path, created_at, last_opened_at, is_pinned "
            "FROM project WHERE id = ?;"
        );
        return data::sqlite::SqlHelper::queryOne<domain::project::Project>(sql, {id.toString()}, db, mapProject);
    }

    std::optional<domain::project::Project> SqliteProjectRepository::getProjectByPath(const QString &rootPath) {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "SELECT id, name, root_path, created_at, last_opened_at, is_pinned "
            "FROM project WHERE root_path = ?;"
        );
        return data::sqlite::SqlHelper::queryOne<domain::project::Project>(sql, {rootPath}, db, mapProject);
    }

    void SqliteProjectRepository::saveProject(const domain::project::Project &p) {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral(
            "INSERT OR REPLACE INTO project (id, name, root_path, created_at, last_opened_at, is_pinned) "
            "VALUES (?, ?, ?, ?, ?, ?);"
        );
        const QVariantList args = {
            p.id.toString(),
            p.name,
            p.rootPath,
            p.createdAt.toMSecsSinceEpoch(),
            p.lastOpenedAt.toMSecsSinceEpoch(),
            p.isPinned ? 1 : 0
        };
        const auto result = data::sqlite::SqlHelper::execute(sql, args, db);
        if (!result) {
            qWarning().noquote() << QStringLiteral("[SqliteProjectRepository] saveProject failed: %1")
                .arg(result.error.message);
        }
    }

    void SqliteProjectRepository::deleteProject(const QUuid &id) {
        auto db = QSqlDatabase::database(m_connectionName);
        const QString sql = QStringLiteral("DELETE FROM project WHERE id = ?;");
        const auto result = data::sqlite::SqlHelper::execute(sql, {id.toString()}, db);
        if (!result) {
            qWarning().noquote() << QStringLiteral("[SqliteProjectRepository] deleteProject failed: %1")
                .arg(result.error.message);
        }
    }

} // namespace data::repository
