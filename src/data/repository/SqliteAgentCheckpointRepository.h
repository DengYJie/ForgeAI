#pragma once

#include <QString>
#include "domain/repository/IAgentCheckpointRepository.h"

namespace data::repository {

    /**
     * @brief 基于 SQLite 的 Agent 检查点快照持久化仓储实现
     */
    class SqliteAgentCheckpointRepository final : public domain::repository::IAgentCheckpointRepository {
    public:
        explicit SqliteAgentCheckpointRepository(const QString& connectionName = QStringLiteral("forgeai_db"));
        ~SqliteAgentCheckpointRepository() override = default;

        bool initializeDatabase();

        bool saveCheckpoint(const domain::agent::AgentCheckpoint& checkpoint) override;
        std::optional<domain::agent::AgentCheckpoint> getLatestCheckpoint(const QString& sessionId) const override;
        bool deleteCheckpointsForSession(const QString& sessionId) override;

    private:
        QString m_connectionName;
    };

} // namespace data::repository
