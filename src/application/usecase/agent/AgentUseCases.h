#pragma once

#include "application/usecase/agent/RunAgentUseCase.h"
#include "application/usecase/agent/CancelAgentRunUseCase.h"
#include "application/usecase/agent/ResumeAgentRunUseCase.h"

namespace application::usecase::agent {

    /**
     * @brief Agent 业务用例聚合容器
     */
    struct AgentUseCases {
        RunAgentUseCase* runAgent = nullptr;
        CancelAgentRunUseCase* cancelAgentRun = nullptr;
        ResumeAgentRunUseCase* resumeAgentRun = nullptr;
    };

} // namespace application::usecase::agent
