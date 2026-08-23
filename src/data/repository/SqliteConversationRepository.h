#pragma once
#include <QSqlDatabase>
#include <QString>
#include "domain/repository/IConversationRepository.h"

namespace data::repository {
    /**
     * @brief 基于 SQLite 的会话元数据仓储实现
     */
    class SqliteConversationRepository : public domain::repository::IConversationRepository {
    public:
        explicit SqliteConversationRepository(const QString &connectionName = "forgeai_db");

        ~SqliteConversationRepository() override;

        /**
         * @brief 初始化数据库结构（创建 conversation 和 turn 表）
         * @return 成功返回 true，失败返回 false
         */
        bool initializeDatabase();

        QList<domain::conversation::Conversation> getAllConversations() override;

        std::optional<domain::conversation::Conversation> getConversation(const QUuid &id) override;

        void saveConversation(const domain::conversation::Conversation &conversation) override;

        void deleteConversation(const QUuid &id) override;


    private:
        QString m_connectionName;
    };
} // namespace data::repository
