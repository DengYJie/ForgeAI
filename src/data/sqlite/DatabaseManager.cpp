#include "DatabaseManager.h"
#include "SqlHelper.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>

namespace data::sqlite {

    DatabaseManager &DatabaseManager::instance() {
        static DatabaseManager instance;
        return instance;
    }

    DatabaseManager::~DatabaseManager() {
        close();
    }

    QString DatabaseManager::getDefaultDatabasePath() {
        return QDir::homePath() + QStringLiteral("/.forgeai/database/forgeai.db");
    }

    bool DatabaseManager::initialize(const QString &customDbPath, const QString &connectionName) {
        m_connectionName = connectionName;
        m_dbPath = customDbPath.isEmpty() ? getDefaultDatabasePath() : customDbPath;

        // 确保父目录存在
        QFileInfo dbFileInfo(m_dbPath);
        QDir dir = dbFileInfo.dir();
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }

        // 移除旧连接以防重复初始化
        if (QSqlDatabase::contains(m_connectionName)) {
            QSqlDatabase::removeDatabase(m_connectionName);
        }

        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(m_dbPath);

        if (!db.open()) {
            return false;
        }

        // 开启 WAL 模式、外键约束、正常同步和忙等待超时
        SqlHelper::exec(QStringLiteral("PRAGMA journal_mode = WAL;"), db);
        SqlHelper::exec(QStringLiteral("PRAGMA synchronous = NORMAL;"), db);
        SqlHelper::exec(QStringLiteral("PRAGMA foreign_keys = ON;"), db);
        SqlHelper::exec(QStringLiteral("PRAGMA busy_timeout = 5000;"), db);

        return true;
    }

    QSqlDatabase DatabaseManager::database() const {
        return QSqlDatabase::database(m_connectionName);
    }

    void DatabaseManager::close() {
        if (QSqlDatabase::contains(m_connectionName)) {
            auto db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen()) {
                db.close();
            }
        }
    }

} // namespace data::sqlite
