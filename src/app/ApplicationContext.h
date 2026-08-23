#pragma once

#include <memory>
#include "data/sqlite/DatabaseManager.h"
#include "data/repository/SqliteConversationRepository.h"
#include "services/conversation/ConversationService.h"
#include "services/chat/ChatService.h"
#include "application/usecase/chat/ChatUseCases.h"

namespace app {
    /**
     * @brief 应用程序级组合根 (Composition Root)
     * @details 在 main 栈上统一构造与编排基础设施、仓储、服务与业务用例，
     *          通过构造函数注入传递给 UI / ViewModels，实现标准的 Clean Architecture。
     */
    class ApplicationContext {
    public:
        ApplicationContext();
        ~ApplicationContext();

        // 1. 基础设施层
        data::sqlite::DatabaseManager &dbManager();

        // 2. 仓储层
        domain::repository::IConversationRepository *conversationRepository() const;

        // 3. 服务层
        domain::service::IConversationService *conversationService() const;
        domain::service::IChatService *chatService() const;

        // 4. UseCase 业务用例层
        application::usecase::chat::SendMessageUseCase *sendMessageUseCase() const;
        application::usecase::chat::StopGenerationUseCase *stopGenerationUseCase() const;
        application::usecase::conversation::LoadSessionsUseCase *loadSessionsUseCase() const;
        application::usecase::conversation::LoadSessionDetailUseCase *loadSessionDetailUseCase() const;
        application::usecase::conversation::CreateSessionUseCase *createSessionUseCase() const;
        application::usecase::conversation::DeleteSessionUseCase *deleteSessionUseCase() const;

        /**
         * @brief 获取对话界面的完整 UseCase 聚合包
         */
        application::usecase::chat::ChatUseCases chatUseCases() const;

    private:
        // 仓储与服务实现
        std::unique_ptr<data::repository::SqliteConversationRepository> m_conversationRepo;
        std::unique_ptr<services::conversation::ConversationService> m_conversationService;
        std::unique_ptr<services::chat::ChatService> m_chatService;

        // 业务用例
        std::unique_ptr<application::usecase::chat::SendMessageUseCase> m_sendMessageUseCase;
        std::unique_ptr<application::usecase::chat::StopGenerationUseCase> m_stopGenerationUseCase;
        std::unique_ptr<application::usecase::conversation::LoadSessionsUseCase> m_loadSessionsUseCase;
        std::unique_ptr<application::usecase::conversation::LoadSessionDetailUseCase> m_loadSessionDetailUseCase;
        std::unique_ptr<application::usecase::conversation::CreateSessionUseCase> m_createSessionUseCase;
        std::unique_ptr<application::usecase::conversation::DeleteSessionUseCase> m_deleteSessionUseCase;
    };
} // namespace app
