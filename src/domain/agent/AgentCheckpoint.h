#pragma once

#include <QUuid>
#include <QString>
#include <QList>
#include <QDateTime>
#include "AgentRunState.h"
#include "ToolExecution.h"
#include "domain/conversation/Message.h"

namespace domain::agent {

    /**
     * @brief Agent 执行状态快照 / 断点检查点实体
     */
    struct AgentCheckpoint {
        QUuid checkpointId;
        QString sessionId;
        QUuid runId;
        int roundIndex = 0;
        AgentRunStatus status = AgentRunStatus::Idle;
        QList<domain::conversation::Message> messages;
        QList<domain::agent::ToolCall> pendingToolCalls;
        QList<domain::agent::ToolResult> pendingToolResults;
        QDateTime timestamp;

        bool operator==(const AgentCheckpoint& other) const = default;
    };

} // namespace domain::agent
