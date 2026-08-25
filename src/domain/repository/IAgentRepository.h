#pragma once

#include <QUuid>
#include <QList>
#include <optional>
#include "domain/agent/Agent.h"

namespace domain::repository {

    /**
     * @brief Agent 持久化仓储接口
     */
    class IAgentRepository {
    public:
        virtual ~IAgentRepository() = default;

        virtual std::optional<domain::agent::Agent> getAgent(const QUuid& id) const = 0;
        virtual QList<domain::agent::Agent> getAllAgents() const = 0;
        virtual bool saveAgent(const domain::agent::Agent& agent) = 0;
        virtual bool deleteAgent(const QUuid& id) = 0;
    };

} // namespace domain::repository
