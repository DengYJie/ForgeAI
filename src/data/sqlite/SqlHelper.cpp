#include "SqlHelper.h"
#include <QSqlQuery>
#include <QSqlError>

namespace data::sqlite {

    bool SqlHelper::exec(const QString &sql, const QSqlDatabase &db) {
        if (!db.isOpen()) {
            return false;
        }
        QSqlQuery query(db);
        return query.exec(sql);
    }

    bool SqlHelper::exec(const QString &sql, const QVariantList &args, const QSqlDatabase &db) {
        if (!db.isOpen()) {
            return false;
        }
        QSqlQuery query(db);
        query.prepare(sql);
        for (int i = 0; i < args.size(); ++i) {
            query.bindValue(i, args[i]);
        }
        return query.exec();
    }

    int SqlHelper::scalarInt(const QString &sql, const QVariantList &args, const QSqlDatabase &db, int defaultValue) {
        if (!db.isOpen()) {
            return defaultValue;
        }
        QSqlQuery query(db);
        query.prepare(sql);
        for (int i = 0; i < args.size(); ++i) {
            query.bindValue(i, args[i]);
        }
        if (query.exec() && query.next()) {
            return query.value(0).toInt();
        }
        return defaultValue;
    }

    QString SqlHelper::scalarString(const QString &sql, const QVariantList &args, const QSqlDatabase &db, const QString &defaultValue) {
        if (!db.isOpen()) {
            return defaultValue;
        }
        QSqlQuery query(db);
        query.prepare(sql);
        for (int i = 0; i < args.size(); ++i) {
            query.bindValue(i, args[i]);
        }
        if (query.exec() && query.next()) {
            return query.value(0).toString();
        }
        return defaultValue;
    }

    bool SqlHelper::tableExists(const QString &tableName, const QSqlDatabase &db) {
        if (!db.isOpen()) {
            return false;
        }
        const QString sql = QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?;");
        return scalarInt(sql, {tableName}, db, 0) == 1;
    }

} // namespace data::sqlite
