#include "SqlTransaction.h"

namespace data::sqlite {

    SqlTransaction::SqlTransaction(QSqlDatabase db)
        : m_db(db) {
        if (m_db.isOpen()) {
            m_started = m_db.transaction();
        }
    }

    SqlTransaction::~SqlTransaction() {
        if (m_started && !m_committed) {
            rollback();
        }
    }

    SqlTransaction::SqlTransaction(SqlTransaction &&other) noexcept
        : m_db(other.m_db), m_started(other.m_started), m_committed(other.m_committed) {
        other.m_started = false;
        other.m_committed = true;
    }

    SqlTransaction &SqlTransaction::operator=(SqlTransaction &&other) noexcept {
        if (this != &other) {
            if (m_started && !m_committed) {
                rollback();
            }
            m_db = other.m_db;
            m_started = other.m_started;
            m_committed = other.m_committed;

            other.m_started = false;
            other.m_committed = true;
        }
        return *this;
    }

    bool SqlTransaction::commit() {
        if (m_started && !m_committed) {
            m_committed = m_db.commit();
            return m_committed;
        }
        return false;
    }

    void SqlTransaction::rollback() {
        if (m_started && !m_committed) {
            m_db.rollback();
            m_committed = true; // 标记已结束
        }
    }

    bool SqlTransaction::isStarted() const {
        return m_started;
    }

} // namespace data::sqlite
