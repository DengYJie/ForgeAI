#pragma once

#include "application/usecase/work/StartTaskUseCase.h"
#include "application/usecase/work/CancelTaskUseCase.h"
#include "application/usecase/chat/SendMessageUseCase.h"

namespace domain::service { class IAgentToolService; }
namespace application::usecase::work {
    /**
     * @brief 工作流界面业务用例聚合容器
     */
    struct WorkUseCases {
        StartTaskUseCase *startTask = nullptr;
        CancelTaskUseCase *cancelTask = nullptr;
        // A dedicated instance keeps Work agent streaming independent from the
        // normal ChatPage conversation operation.
        application::usecase::chat::SendMessageUseCase *agentConversation = nullptr;
        domain::service::IAgentToolService *agentTools = nullptr;
    };
} // namespace application::usecase::work
