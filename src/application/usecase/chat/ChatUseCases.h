#pragma once

#include "application/usecase/chat/SendMessageUseCase.h"
#include "application/usecase/chat/StopGenerationUseCase.h"
#include "application/usecase/conversation/LoadSessionsUseCase.h"
#include "application/usecase/conversation/LoadSessionDetailUseCase.h"
#include "application/usecase/conversation/CreateSessionUseCase.h"
#include "application/usecase/conversation/DeleteSessionUseCase.h"
#include "application/usecase/conversation/ClearSessionUseCase.h"
#include "application/usecase/conversation/SetSessionPinnedUseCase.h"
#include "application/usecase/conversation/SetSessionArchivedUseCase.h"
#include "application/usecase/conversation/SetSessionTitleUseCase.h"
#include "application/usecase/settings/GetModelsUseCase.h"

namespace application::usecase::chat {
    /**
     * @brief 对话界面业务用例聚合容器 (UseCase Bundle)
     * @details 聚合 ChatViewModel 所需的所有单一职责用例指针，简化依赖传递并保持各用例独立性。
     */
    struct ChatUseCases {
        SendMessageUseCase *sendMessage = nullptr;                             ///< 发送消息并流式生成回复用例
        StopGenerationUseCase *stopGeneration = nullptr;                       ///< 中止大模型生成任务用例
        conversation::LoadSessionsUseCase *loadSessions = nullptr;             ///< 加载会话元数据列表用例
        conversation::LoadSessionDetailUseCase *loadSessionDetail = nullptr;   ///< 加载会话历史消息流详情用例
        conversation::CreateSessionUseCase *createSession = nullptr;           ///< 占位复用/新建会话用例
        conversation::DeleteSessionUseCase *deleteSession = nullptr;           ///< 删除会话并邻近回退用例
        conversation::ClearSessionUseCase *clearSession = nullptr;             ///< 清空当前会话消息
        conversation::SetSessionPinnedUseCase *setSessionPinned = nullptr;
        conversation::SetSessionArchivedUseCase *setSessionArchived = nullptr;
        conversation::SetSessionTitleUseCase *setSessionTitle = nullptr;
        settings::GetModelsUseCase *getModels = nullptr;                       ///< 可选模型查询与变更通知
    };
} // namespace application::usecase::chat
