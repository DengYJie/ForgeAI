#pragma once

#include <QString>
#include <optional>
#include "domain/agent/AgentCheckpoint.h"

namespace domain::repository {

    /**
     * @brief Agent 检查点仓储接口
     */
    class IAgentCheckpointRepository {
    public:
        virtual ~IAgentCheckpointRepository() = default;

        virtual bool saveCheckpoint(const domain::agent::AgentCheckpoint& checkpoint) = 0;
        virtual std::optional<domain::agent::AgentCheckpoint> getLatestCheckpoint(const QString& sessionId) const = 0;
        virtual bool deleteCheckpointsForSession(const QString& sessionId) = 0;
    };

} // namespace domain::repository
