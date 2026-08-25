#pragma once

#include "application/usecase/agent/RunAgentUseCase.h"
#include "application/usecase/agent/CancelAgentRunUseCase.h"
#include "application/usecase/settings/GetModelsUseCase.h"

namespace domain::service { class IConversationService; class IModelService; }
namespace domain::repository { class IConversationRepository; class IProjectRepository; }

namespace application::usecase::work {
    class SwitchProjectUseCase;

    /**
     * @brief 工作流界面业务用例聚合容器
     */
    struct WorkUseCases {
        application::usecase::agent::RunAgentUseCase *runAgent = nullptr;
        application::usecase::agent::CancelAgentRunUseCase *cancelAgentRun = nullptr;
        domain::service::IConversationService *conversationService = nullptr;
        domain::repository::IConversationRepository *conversationRepository = nullptr;
        domain::repository::IProjectRepository *projectRepository = nullptr;
        domain::service::IModelService *modelService = nullptr;
        application::usecase::settings::GetModelsUseCase *getModels = nullptr;
        SwitchProjectUseCase *switchProject = nullptr;
    };
} // namespace application::usecase::work
