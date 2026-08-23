#pragma once

#include "application/usecase/chat/SendMessageUseCase.h"
#include "application/usecase/chat/StopGenerationUseCase.h"
#include "application/usecase/conversation/LoadSessionsUseCase.h"
#include "application/usecase/conversation/LoadSessionDetailUseCase.h"
#include "application/usecase/conversation/CreateSessionUseCase.h"
#include "application/usecase/conversation/DeleteSessionUseCase.h"

namespace application::usecase::chat {
    /**
     * @brief 对话界面业务用例聚合容器
     */
    struct ChatUseCases {
        SendMessageUseCase *sendMessage = nullptr;
        StopGenerationUseCase *stopGeneration = nullptr;
        conversation::LoadSessionsUseCase *loadSessions = nullptr;
        conversation::LoadSessionDetailUseCase *loadSessionDetail = nullptr;
        conversation::CreateSessionUseCase *createSession = nullptr;
        conversation::DeleteSessionUseCase *deleteSession = nullptr;
    };
} // namespace application::usecase::chat
