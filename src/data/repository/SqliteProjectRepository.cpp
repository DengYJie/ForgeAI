#include "SqliteProjectRepository.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

namespace data::repository {
SqliteProjectRepository::SqliteProjectRepository(const QString& connectionName) : m_connectionName(connectionName) {}
bool SqliteProjectRepository::initializeDatabase() {
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    return q.exec("CREATE TABLE IF NOT EXISTS project (id TEXT PRIMARY KEY,name TEXT NOT NULL,root_path TEXT UNIQUE NOT NULL,created_at INTEGER,last_opened_at INTEGER)");
}
QList<domain::project::Project> SqliteProjectRepository::getAllProjects() {
    QList<domain::project::Project> out; QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.exec("SELECT id,name,root_path,created_at,last_opened_at FROM project ORDER BY last_opened_at DESC");
    while (q.next()) { domain::project::Project p; p.id=QUuid(q.value(0).toString()); p.name=q.value(1).toString(); p.rootPath=q.value(2).toString(); p.createdAt=QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong()); p.lastOpenedAt=QDateTime::fromMSecsSinceEpoch(q.value(4).toLongLong()); out.append(p); } return out;
}
std::optional<domain::project::Project> SqliteProjectRepository::getProject(const QUuid& id) { for (auto& p:getAllProjects()) if(p.id==id) return p; return {}; }
std::optional<domain::project::Project> SqliteProjectRepository::getProjectByPath(const QString& path) { for (auto& p:getAllProjects()) if(p.rootPath==path) return p; return {}; }
void SqliteProjectRepository::saveProject(const domain::project::Project& p) { QSqlQuery q(QSqlDatabase::database(m_connectionName)); q.prepare("INSERT OR REPLACE INTO project(id,name,root_path,created_at,last_opened_at) VALUES(?,?,?,?,?)"); q.addBindValue(p.id.toString()); q.addBindValue(p.name); q.addBindValue(p.rootPath); q.addBindValue(p.createdAt.toMSecsSinceEpoch()); q.addBindValue(p.lastOpenedAt.toMSecsSinceEpoch()); q.exec(); }
void SqliteProjectRepository::deleteProject(const QUuid& id) { QSqlQuery q(QSqlDatabase::database(m_connectionName)); q.prepare("DELETE FROM project WHERE id=?"); q.addBindValue(id.toString()); q.exec(); }
}
