#pragma once
#include <QSqlDatabase>

namespace data::sqlite {

    /**
     * @brief SQLite 事务 RAII 卫士
     * @details 在构造时开启事务，在析构时若未显式 commit 则自动执行 rollback，杜绝数据库死锁。
     */
    class SqlTransaction {
    public:
        explicit SqlTransaction(QSqlDatabase db = QSqlDatabase::database());
        ~SqlTransaction();

        SqlTransaction(const SqlTransaction &) = delete;
        SqlTransaction &operator=(const SqlTransaction &) = delete;

        SqlTransaction(SqlTransaction &&other) noexcept;
        SqlTransaction &operator=(SqlTransaction &&other) noexcept;

        /**
         * @brief 提交事务
         * @return 成功返回 true，失败返回 false
         */
        bool commit();

        /**
         * @brief 显式回滚事务
         */
        void rollback();

        /**
         * @brief 检查事务是否已成功开启
         */
        bool isStarted() const;

    private:
        QSqlDatabase m_db;
        bool m_started = false;
        bool m_committed = false;
    };

} // namespace data::sqlite
