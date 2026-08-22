#pragma once
#include <QSqlDatabase>
#include <QString>
#include <QVariant>
#include <QVariantList>

namespace data::sqlite {

    /**
     * @brief SQLite 快捷操作静态工具集
     */
    class SqlHelper {
    public:
        /**
         * @brief 快速执行无参数的 SQL 语句（如 DDL 建表）
         */
        static bool exec(const QString &sql, const QSqlDatabase &db = QSqlDatabase::database());

        /**
         * @brief 快速执行带绑定参数的 SQL 语句
         */
        static bool exec(const QString &sql, const QVariantList &args, const QSqlDatabase &db = QSqlDatabase::database());

        /**
         * @brief 执行查询并返回首行首列的整数（常用于 COUNT(*)、SUM 等聚合查询）
         */
        static int scalarInt(const QString &sql, const QVariantList &args = {}, const QSqlDatabase &db = QSqlDatabase::database(), int defaultValue = 0);

        /**
         * @brief 执行查询并返回首行首列的字符串
         */
        static QString scalarString(const QString &sql, const QVariantList &args = {}, const QSqlDatabase &db = QSqlDatabase::database(), const QString &defaultValue = {});

        /**
         * @brief 检查指定数据表是否存在
         */
        static bool tableExists(const QString &tableName, const QSqlDatabase &db = QSqlDatabase::database());
    };

} // namespace data::sqlite
