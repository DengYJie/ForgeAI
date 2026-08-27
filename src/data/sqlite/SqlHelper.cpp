#include "SqlHelper.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>

namespace data::sqlite {

    SqlError SqlHelper::validateDatabase(const QSqlDatabase &db) {
        SqlError err;
        if (!db.isValid()) {
            err.type = QSqlError::ConnectionError;
            err.message = QStringLiteral("Database connection is invalid");
            return err;
        }
        if (!db.isOpen()) {
            err.type = QSqlError::ConnectionError;
            err.message = QStringLiteral("Database is not open");
            return err;
        }
        if (!db.driver()) {
            err.type = QSqlError::ConnectionError;
            err.message = QStringLiteral("Database driver is unavailable");
            return err;
        }
        return err;
    }

    SqlError SqlHelper::extractError(const QSqlQuery &query, const QString &sql, const QString &contextMsg) {
        const auto lastErr = query.lastError();
        SqlError err;
        err.type = lastErr.type();
        err.databaseText = lastErr.databaseText();
        err.driverText = lastErr.driverText();
        err.sql = sql;

        QString msg = contextMsg.isEmpty() ? QStringLiteral("SQL operation failed") : contextMsg;
        if (!err.databaseText.isEmpty()) {
            msg += QStringLiteral(": %1").arg(err.databaseText);
        } else if (!err.driverText.isEmpty()) {
            msg += QStringLiteral(": %1").arg(err.driverText);
        } else if (!lastErr.text().trimmed().isEmpty()) {
            msg += QStringLiteral(": %1").arg(lastErr.text().trimmed());
        }
        err.message = msg;
        return err;
    }

    bool SqlHelper::prepareAndBind(QSqlQuery &query, const QString &sql, const QVariantList &args, SqlError *error) {
        if (!query.prepare(sql)) {
            if (error) {
                *error = extractError(query, sql, QStringLiteral("Prepare failed"));
            }
            return false;
        }
        for (int i = 0; i < args.size(); ++i) {
            query.bindValue(i, args.at(i));
        }
        return true;
    }

    bool SqlHelper::prepareAndBind(QSqlQuery &query, const QString &sql, const QVariantMap &args, SqlError *error) {
        if (!query.prepare(sql)) {
            if (error) {
                *error = extractError(query, sql, QStringLiteral("Prepare failed"));
            }
            return false;
        }
        for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
            query.bindValue(it.key(), it.value());
        }
        return true;
    }

    SqlExecResult SqlHelper::execute(const QString &sql, const QSqlDatabase &db) {
        return execute(sql, QVariantList{}, db);
    }

    SqlExecResult SqlHelper::execute(const QString &sql, const QVariantList &args, const QSqlDatabase &db) {
        SqlExecResult result;
        result.error = validateDatabase(db);
        if (result.error.isValid()) {
            return result;
        }

        QSqlQuery query(db);
        if (!prepareAndBind(query, sql, args, &result.error)) {
            return result;
        }

        if (!query.exec()) {
            result.error = extractError(query, sql, QStringLiteral("Execute failed"));
            return result;
        }

        result.success = true;
        result.affectedRows = query.numRowsAffected();
        result.lastInsertId = query.lastInsertId();
        return result;
    }

    SqlExecResult SqlHelper::execute(const QString &sql, const QVariantMap &args, const QSqlDatabase &db) {
        SqlExecResult result;
        result.error = validateDatabase(db);
        if (result.error.isValid()) {
            return result;
        }

        QSqlQuery query(db);
        if (!prepareAndBind(query, sql, args, &result.error)) {
            return result;
        }

        if (!query.exec()) {
            result.error = extractError(query, sql, QStringLiteral("Execute failed"));
            return result;
        }

        result.success = true;
        result.affectedRows = query.numRowsAffected();
        result.lastInsertId = query.lastInsertId();
        return result;
    }

    SqlExecResult SqlHelper::executeBatch(const QString &sql, const QList<QVariantList> &rows, const QSqlDatabase &db) {
        SqlExecResult result;
        if (rows.isEmpty()) {
            result.success = true;
            return result;
        }

        result.error = validateDatabase(db);
        if (result.error.isValid()) {
            return result;
        }

        QSqlQuery query(db);
        if (!query.prepare(sql)) {
            result.error = extractError(query, sql, QStringLiteral("Batch prepare failed"));
            return result;
        }

        const int numCols = static_cast<int>(rows.first().size());
        const int numRows = static_cast<int>(rows.size());
        for (int c = 0; c < numCols; ++c) {
            QVariantList colValues;
            colValues.reserve(numRows);
            for (int r = 0; r < numRows; ++r) {
                colValues.append(c < rows[r].size() ? rows[r][c] : QVariant());
            }
            query.addBindValue(colValues);
        }

        if (!query.execBatch()) {
            result.error = extractError(query, sql, QStringLiteral("Batch execute failed"));
            return result;
        }

        result.success = true;
        // SQLite 驱动的 numRowsAffected() 仅反映最后一条执行语句的影响行数 (1)。
        // 当 execBatch 整体成功返回时，实际影响行数为该批次提交的总行数。
        const qint64 driverAffected = query.numRowsAffected();
        result.affectedRows = (driverAffected > numRows) ? driverAffected : static_cast<qint64>(numRows);
        return result;
    }

    QVariant SqlHelper::scalar(const QString &sql, const QSqlDatabase &db, const QVariant &defaultValue) {
        return scalar(sql, QVariantList{}, db, defaultValue);
    }

    QVariant SqlHelper::scalar(const QString &sql, const QVariantList &args, const QSqlDatabase &db, const QVariant &defaultValue) {
        SqlError err = validateDatabase(db);
        if (err.isValid()) {
            return defaultValue;
        }

        QSqlQuery query(db);
        if (!prepareAndBind(query, sql, args, &err)) {
            return defaultValue;
        }

        if (query.exec() && query.next()) {
            return query.value(0);
        }

        return defaultValue;
    }

    bool SqlHelper::tableExists(const QString &tableName, const QSqlDatabase &db) {
        if (tableName.isEmpty()) return false;
        const QString sql = QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?;");
        return scalarAs<int>(sql, {tableName}, db, 0) == 1;
    }

    bool SqlHelper::columnExists(const QString &tableName, const QString &columnName, const QSqlDatabase &db) {
        if (tableName.isEmpty() || columnName.isEmpty()) return false;
        const auto cols = columns(tableName, db);
        for (const auto &col : cols) {
            if (col.compare(columnName, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    }

    QStringList SqlHelper::columns(const QString &tableName, const QSqlDatabase &db) {
        QStringList list;
        if (tableName.isEmpty() || validateDatabase(db).isValid()) {
            return list;
        }

        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("PRAGMA table_info(%1);").arg(tableName))) {
            return list;
        }

        while (query.next()) {
            list.append(query.value(1).toString());
        }
        return list;
    }

    bool SqlHelper::indexExists(const QString &indexName, const QSqlDatabase &db) {
        if (indexName.isEmpty()) return false;
        const QString sql = QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'index' AND name = ?;");
        return scalarAs<int>(sql, {indexName}, db, 0) == 1;
    }

} // namespace data::sqlite
