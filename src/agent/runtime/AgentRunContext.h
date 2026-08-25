#pragma once

#include <QUuid>
#include <QString>
#include "domain/model/ModelProvider.h"
#include "domain/agent/AgentPolicy.h"

namespace agent::runtime {

    /**
     * @brief Agent 单次运行的上下文配置参数
     */
    struct AgentRunContext {
        QUuid runId;
        QString sessionId;
        QUuid projectId;
        QString workspaceRoot;

        QString systemPrompt;
        domain::model::ModelProvider provider;
        QString modelId;

        bool useWebSearch = false;
        bool useDeepThinking = false;
        QString reasoningEffort;

        domain::agent::AgentPolicy policy;
    };

} // namespace agent::runtime
