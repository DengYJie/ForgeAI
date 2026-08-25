#pragma once

#include "application/usecase/chat/SendMessageUseCase.h"

namespace domain::service { class IAgentToolService; }
namespace domain::service { class IConversationService; }
namespace domain::repository { class IConversationRepository; class IProjectRepository; }
namespace application::usecase::work {
    /**
     * @brief 工作流界面业务用例聚合容器
     */
    struct WorkUseCases {
        // A dedicated instance keeps Work agent streaming independent from the
        // normal ChatPage conversation operation.
        application::usecase::chat::SendMessageUseCase *agentConversation = nullptr;
        domain::service::IAgentToolService *agentTools = nullptr;
        domain::service::IConversationService *conversationService = nullptr;
        domain::repository::IConversationRepository *conversationRepository = nullptr;
        domain::repository::IProjectRepository *projectRepository = nullptr;
    };
} // namespace application::usecase::work
