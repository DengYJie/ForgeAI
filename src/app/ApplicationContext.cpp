#include "ApplicationContext.h"

namespace app {
    ApplicationContext::ApplicationContext() {
        // 1. 仓储与基础服务初始化
        m_conversationRepo = std::make_unique<data::repository::SqliteConversationRepository>();
        m_conversationService = std::make_unique<services::conversation::ConversationService>(m_conversationRepo.get());
        m_chatService = std::make_unique<services::chat::ChatService>();

        // 2. UseCase 业务用例编排初始化
        m_sendMessageUseCase = std::make_unique<application::usecase::chat::SendMessageUseCase>(
            m_chatService.get(),
            m_conversationService.get()
        );
        m_stopGenerationUseCase = std::make_unique<application::usecase::chat::StopGenerationUseCase>(
            m_chatService.get()
        );
        m_loadSessionsUseCase = std::make_unique<application::usecase::conversation::LoadSessionsUseCase>(
            m_conversationService.get()
        );
        m_loadSessionDetailUseCase = std::make_unique<application::usecase::conversation::LoadSessionDetailUseCase>(
            m_conversationService.get()
        );
        m_createSessionUseCase = std::make_unique<application::usecase::conversation::CreateSessionUseCase>(
            m_conversationService.get()
        );
        m_deleteSessionUseCase = std::make_unique<application::usecase::conversation::DeleteSessionUseCase>(
            m_conversationService.get()
        );

        // 3. ViewModel 表现层构造（直接注入 UseCases）
        m_chatViewModel = std::make_unique<ui::screen::chat::ChatViewModel>(chatUseCases());
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

    application::usecase::chat::SendMessageUseCase *ApplicationContext::sendMessageUseCase() const {
        return m_sendMessageUseCase.get();
    }

    application::usecase::chat::StopGenerationUseCase *ApplicationContext::stopGenerationUseCase() const {
        return m_stopGenerationUseCase.get();
    }

    application::usecase::conversation::LoadSessionsUseCase *ApplicationContext::loadSessionsUseCase() const {
        return m_loadSessionsUseCase.get();
    }

    application::usecase::conversation::LoadSessionDetailUseCase *ApplicationContext::loadSessionDetailUseCase() const {
        return m_loadSessionDetailUseCase.get();
    }

    application::usecase::conversation::CreateSessionUseCase *ApplicationContext::createSessionUseCase() const {
        return m_createSessionUseCase.get();
    }

    application::usecase::conversation::DeleteSessionUseCase *ApplicationContext::deleteSessionUseCase() const {
        return m_deleteSessionUseCase.get();
    }

    application::usecase::chat::ChatUseCases ApplicationContext::chatUseCases() const {
        application::usecase::chat::ChatUseCases c;
        c.sendMessage = m_sendMessageUseCase.get();
        c.stopGeneration = m_stopGenerationUseCase.get();
        c.loadSessions = m_loadSessionsUseCase.get();
        c.loadSessionDetail = m_loadSessionDetailUseCase.get();
        c.createSession = m_createSessionUseCase.get();
        c.deleteSession = m_deleteSessionUseCase.get();
        return c;
    }

    ui::screen::chat::ChatViewModel *ApplicationContext::chatViewModel() const {
        return m_chatViewModel.get();
    }
} // namespace app
