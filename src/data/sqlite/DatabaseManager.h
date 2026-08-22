#pragma once
#include <QSqlDatabase>
#include <QString>

namespace data::sqlite {

    /**
     * @brief SQLite 数据库全局连接与性能生命周期管理器
     */
    class DatabaseManager {
    public:
        static DatabaseManager &instance();

        /**
         * @brief 初始化全局数据库连接并配置高性能 WAL 模式与外键约束
         * @param customDbPath 可选的自定义数据库文件路径（留空则使用 ~/.forgeai/database/forgeai.db）
         * @param connectionName 连接标识符（默认 "forgeai_db"）
         * @return 成功返回 true，失败返回 false
         */
        bool initialize(const QString &customDbPath = {}, const QString &connectionName = QStringLiteral("forgeai_db"));

        /**
         * @brief 获取当前有效的数据库连接句柄
         */
        QSqlDatabase database() const;

        /**
         * @brief 获取默认的数据库文件存放路径
         */
        static QString getDefaultDatabasePath();

        /**
         * @brief 关闭数据库连接
         */
        void close();

    private:
        DatabaseManager() = default;
        ~DatabaseManager();
        DatabaseManager(const DatabaseManager &) = delete;
        DatabaseManager &operator=(const DatabaseManager &) = delete;

        QString m_connectionName = QStringLiteral("forgeai_db");
        QString m_dbPath;
    };

} // namespace data::sqlite
