#pragma once

#include <QString>
#include "domain/repository/IAgentRepository.h"

namespace data::repository {

    /**
     * @brief 基于 SQLite 的 Agent 仓储实现
     */
    class SqliteAgentRepository final : public domain::repository::IAgentRepository {
    public:
        explicit SqliteAgentRepository(const QString& connectionName = QStringLiteral("forgeai_db"));
        ~SqliteAgentRepository() override = default;

        bool initializeDatabase();

        std::optional<domain::agent::Agent> getAgent(const QUuid& id) const override;
        QList<domain::agent::Agent> getAllAgents() const override;
        bool saveAgent(const domain::agent::Agent& agent) override;
        bool deleteAgent(const QUuid& id) override;

    private:
        QString m_connectionName;
    };

} // namespace data::repository
