#include "ApplicationContext.h"

namespace app {
    ApplicationContext::ApplicationContext() {
        m_conversationRepo = std::make_unique<data::repository::SqliteConversationRepository>();
        m_conversationService = std::make_unique<services::conversation::ConversationService>(m_conversationRepo.get());
        m_chatService = std::make_unique<services::chat::ChatService>();
    }

    ApplicationContext::~ApplicationContext() = default;

    data::sqlite::DatabaseManager &ApplicationContext::dbManager() {
        return data::sqlite::DatabaseManager::instance();
    }

    domain::repository::IConversationRepository *ApplicationContext::conversationRepository() const {
        return m_conversationRepo.get();
    }

    domain::service::IConversationService *ApplicationContext::conversationService() const {
        return m_conversationService.get();
    }

    domain::service::IChatService *ApplicationContext::chatService() const {
        return m_chatService.get();
    }
} // namespace app
