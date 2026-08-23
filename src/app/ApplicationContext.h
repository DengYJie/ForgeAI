#pragma once

#include <memory>
#include "data/sqlite/DatabaseManager.h"
#include "data/repository/SqliteConversationRepository.h"
#include "services/conversation/ConversationService.h"
#include "services/chat/ChatService.h"

namespace app {
    /**
     * @brief 应用程序级组合根 (Composition Root)
     * @details 在 main 栈上统一构造与编排基础设施、仓储与领域服务，
     *          通过构造函数注入传递给 UI / ViewModels，实现清晰的单向依赖与生命周期管理。
     */
    class ApplicationContext {
    public:
        ApplicationContext();
        ~ApplicationContext();

        // 基础设施层
        data::sqlite::DatabaseManager &dbManager();

        // 仓储层
        domain::repository::IConversationRepository *conversationRepository() const;

        // 服务层
        domain::service::IConversationService *conversationService() const;
        domain::service::IChatService *chatService() const;

    private:
        std::unique_ptr<data::repository::SqliteConversationRepository> m_conversationRepo;
        std::unique_ptr<services::conversation::ConversationService> m_conversationService;
        std::unique_ptr<services::chat::ChatService> m_chatService;
    };
} // namespace app
