#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QList>
#include <optional>
#include <utility>

namespace data::sqlite {

    /**
     * @brief 结构化 SQL 错误模型
     */
    struct SqlError {
        QString message;                    ///< 聚合可读错误描述（包含上下文）
        QString databaseText;               ///< SQLite 引擎原生错误文本
        QString driverText;                 ///< Qt SQL Driver 驱动错误文本
        QString sql;                        ///< 触发错误的原始 SQL 语句
        QSqlError::ErrorType type = QSqlError::NoError;

        bool isValid() const noexcept { return type != QSqlError::NoError || !message.isEmpty(); }
        bool operator==(const SqlError&) const = default;
    };

    /**
     * @brief SQL 执行结果容器
     */
    struct SqlExecResult {
        bool success = false;
        qint64 affectedRows = 0;
        QVariant lastInsertId;
        SqlError error;

        explicit operator bool() const noexcept {
            return success;
        }
    };

    /**
     * @brief SQLite 统一底层执行引擎与元数据辅助工具
     */
    class SqlHelper {
    public:
        /**
         * @brief 执行无参数的 SQL（DML / DDL）
         */
        static SqlExecResult execute(
            const QString &sql,
            const QSqlDatabase &db = QSqlDatabase::database()
        );

        /**
         * @brief 执行带位置参数的 SQL（DML / DDL）
         */
        static SqlExecResult execute(
            const QString &sql,
            const QVariantList &args,
            const QSqlDatabase &db = QSqlDatabase::database()
        );

        /**
         * @brief 执行带命名参数的 SQL（如 :id, :title）
         */
        static SqlExecResult execute(
            const QString &sql,
            const QVariantMap &args,
            const QSqlDatabase &db = QSqlDatabase::database()
        );

        /**
         * @brief 批量执行多行数据插入（基于 QSqlQuery::execBatch）
         * @param sql 含占位符的 SQL（位置参数）
         * @param rows 行数据列表，每行对应一组参数
         */
        static SqlExecResult executeBatch(
            const QString &sql,
            const QList<QVariantList> &rows,
            const QSqlDatabase &db = QSqlDatabase::database()
        );

        /**
         * @brief 兼容旧版 exec()
         */
        static bool exec(const QString &sql, const QSqlDatabase &db = QSqlDatabase::database()) {
            return execute(sql, db).success;
        }

        static bool exec(const QString &sql, const QVariantList &args, const QSqlDatabase &db = QSqlDatabase::database()) {
            return execute(sql, args, db).success;
        }

        /**
         * @brief 单值标量查询（无参数）
         */
        static QVariant scalar(
            const QString &sql,
            const QSqlDatabase &db = QSqlDatabase::database(),
            const QVariant &defaultValue = {}
        );

        /**
         * @brief 单值标量查询（带位置参数）
         */
        static QVariant scalar(
            const QString &sql,
            const QVariantList &args,
            const QSqlDatabase &db = QSqlDatabase::database(),
            const QVariant &defaultValue = {}
        );

        /**
         * @brief 类型安全的标量查询（无参数）
         */
        template<typename T>
        static T scalarAs(
            const QString &sql,
            const QSqlDatabase &db = QSqlDatabase::database(),
            const T &defaultValue = {}
        ) {
            const QVariant value = scalar(sql, db, defaultValue);
            if (!value.isValid() || value.isNull()) {
                return defaultValue;
            }
            return value.value<T>();
        }

        /**
         * @brief 类型安全的标量查询（带位置参数）
         */
        template<typename T>
        static T scalarAs(
            const QString &sql,
            const QVariantList &args,
            const QSqlDatabase &db = QSqlDatabase::database(),
            const T &defaultValue = {}
        ) {
            const QVariant value = scalar(sql, args, db, defaultValue);
            if (!value.isValid() || value.isNull()) {
                return defaultValue;
            }
            return value.value<T>();
        }

        static int scalarInt(const QString &sql, const QSqlDatabase &db = QSqlDatabase::database(), int defaultValue = 0) {
            return scalarAs<int>(sql, db, defaultValue);
        }

        static int scalarInt(const QString &sql, const QVariantList &args, const QSqlDatabase &db = QSqlDatabase::database(), int defaultValue = 0) {
            return scalarAs<int>(sql, args, db, defaultValue);
        }

        static QString scalarString(const QString &sql, const QSqlDatabase &db = QSqlDatabase::database(), const QString &defaultValue = {}) {
            return scalarAs<QString>(sql, db, defaultValue);
        }

        static QString scalarString(const QString &sql, const QVariantList &args, const QSqlDatabase &db = QSqlDatabase::database(), const QString &defaultValue = {}) {
            return scalarAs<QString>(sql, args, db, defaultValue);
        }

        /**
         * @brief 单行记录查询并映射为领域对象（无参数）
         */
        template<typename T, typename Mapper>
        static std::optional<T> queryOne(
            const QString &sql,
            const QSqlDatabase &db,
            Mapper &&mapper
        ) {
            return queryOne<T>(sql, QVariantList{}, db, std::forward<Mapper>(mapper));
        }

        /**
         * @brief 单行记录查询并映射为领域对象（带位置参数）
         */
        template<typename T, typename Mapper>
        static std::optional<T> queryOne(
            const QString &sql,
            const QVariantList &args,
            const QSqlDatabase &db,
            Mapper &&mapper
        ) {
            SqlError err = validateDatabase(db);
            if (err.isValid()) {
                return std::nullopt;
            }

            QSqlQuery query(db);
            if (!prepareAndBind(query, sql, args, &err)) {
                return std::nullopt;
            }

            if (!query.exec()) {
                return std::nullopt;
            }

            if (query.next()) {
                return std::make_optional<T>(mapper(query));
            }

            return std::nullopt;
        }

        /**
         * @brief 多行记录查询并映射为领域对象列表（无参数）
         */
        template<typename T, typename Mapper>
        static QList<T> queryAll(
            const QString &sql,
            const QSqlDatabase &db,
            Mapper &&mapper
        ) {
            return queryAll<T>(sql, QVariantList{}, db, std::forward<Mapper>(mapper));
        }

        /**
         * @brief 多行记录查询并映射为领域对象列表（带位置参数）
         */
        template<typename T, typename Mapper>
        static QList<T> queryAll(
            const QString &sql,
            const QVariantList &args,
            const QSqlDatabase &db,
            Mapper &&mapper
        ) {
            QList<T> list;
            SqlError err = validateDatabase(db);
            if (err.isValid()) {
                return list;
            }

            QSqlQuery query(db);
            if (!prepareAndBind(query, sql, args, &err)) {
                return list;
            }

            if (!query.exec()) {
                return list;
            }

            while (query.next()) {
                list.append(mapper(query));
            }

            return list;
        }

        /**
         * @brief Schema 元数据自省工具
         */
        static bool tableExists(const QString &tableName, const QSqlDatabase &db = QSqlDatabase::database());
        static bool columnExists(const QString &tableName, const QString &columnName, const QSqlDatabase &db = QSqlDatabase::database());
        static QStringList columns(const QString &tableName, const QSqlDatabase &db = QSqlDatabase::database());
        static bool indexExists(const QString &indexName, const QSqlDatabase &db = QSqlDatabase::database());

    private:
        static SqlError validateDatabase(const QSqlDatabase &db);
        static bool prepareAndBind(QSqlQuery &query, const QString &sql, const QVariantList &args, SqlError *error = nullptr);
        static bool prepareAndBind(QSqlQuery &query, const QString &sql, const QVariantMap &args, SqlError *error = nullptr);
        static SqlError extractError(const QSqlQuery &query, const QString &sql, const QString &contextMsg = {});
    };

} // namespace data::sqlite
