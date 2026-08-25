#include "SqliteProjectRepository.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace data::repository {
namespace {
bool hasColumn(const QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) return false;
    while (query.next()) {
        if (query.value(1).toString() == column) return true;
    }
    return false;
}
}

SqliteProjectRepository::SqliteProjectRepository(const QString& connectionName) : m_connectionName(connectionName) {}
bool SqliteProjectRepository::initializeDatabase() {
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    bool ok = q.exec("CREATE TABLE IF NOT EXISTS project (id TEXT PRIMARY KEY,name TEXT NOT NULL,root_path TEXT UNIQUE NOT NULL,created_at INTEGER,last_opened_at INTEGER, is_pinned INTEGER DEFAULT 0)");
    
    // Migration: add is_pinned column to existing project table if it doesn't exist
    if (ok) {
        if (!hasColumn(db, QStringLiteral("project"), QStringLiteral("is_pinned"))) {
            q.exec("ALTER TABLE project ADD COLUMN is_pinned INTEGER DEFAULT 0");
        }
    }
    return ok;
}
QList<domain::project::Project> SqliteProjectRepository::getAllProjects() {
    QList<domain::project::Project> out; QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.exec("SELECT id,name,root_path,created_at,last_opened_at,is_pinned FROM project ORDER BY last_opened_at DESC");
    while (q.next()) { 
        domain::project::Project p; 
        p.id=QUuid(q.value(0).toString()); 
        p.name=q.value(1).toString(); 
        p.rootPath=q.value(2).toString(); 
        p.createdAt=QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong()); 
        p.lastOpenedAt=QDateTime::fromMSecsSinceEpoch(q.value(4).toLongLong()); 
        p.isPinned=q.value(5).toInt() != 0;
        out.append(p); 
    } 
    return out;
}
std::optional<domain::project::Project> SqliteProjectRepository::getProject(const QUuid& id) { for (auto& p:getAllProjects()) if(p.id==id) return p; return {}; }
std::optional<domain::project::Project> SqliteProjectRepository::getProjectByPath(const QString& path) { for (auto& p:getAllProjects()) if(p.rootPath==path) return p; return {}; }
void SqliteProjectRepository::saveProject(const domain::project::Project& p) { 
    QSqlQuery q(QSqlDatabase::database(m_connectionName)); 
    q.prepare("INSERT OR REPLACE INTO project(id,name,root_path,created_at,last_opened_at,is_pinned) VALUES(?,?,?,?,?,?)"); 
    q.addBindValue(p.id.toString()); 
    q.addBindValue(p.name); 
    q.addBindValue(p.rootPath); 
    q.addBindValue(p.createdAt.toMSecsSinceEpoch()); 
    q.addBindValue(p.lastOpenedAt.toMSecsSinceEpoch()); 
    q.addBindValue(p.isPinned ? 1 : 0);
    q.exec(); 
}
void SqliteProjectRepository::deleteProject(const QUuid& id) { QSqlQuery q(QSqlDatabase::database(m_connectionName)); q.prepare("DELETE FROM project WHERE id=?"); q.addBindValue(id.toString()); q.exec(); }
}
