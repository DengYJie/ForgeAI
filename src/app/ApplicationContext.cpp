#include "ApplicationContext.h"
#include "llm/protocol/openai/OpenAIChatCompletionsAdapter.h"
#include "llm/protocol/anthropic/AnthropicProtocolAdapter.h"
#include "llm/protocol/gemini/GeminiProtocolAdapter.h"
#include "llm/protocol/ollama/OllamaProtocolAdapter.h"
#include "llm/protocol/openai_responses/OpenAIResponsesAdapter.h"

namespace app {
    ApplicationContext::ApplicationContext() {
        // 1. 仓储与基础组件初始化
        m_conversationRepo = std::make_unique<data::repository::SqliteConversationRepository>();
        m_modelRepo = std::make_shared<data::repository::SqliteModelRepository>();
        m_modelRegistry = std::make_shared<core::model::ModelRegistry>(m_modelRepo);

        // 1.5 网络与 LLM 协议注册
        m_httpClient = std::make_shared<network::QtHttpClient>();
        m_protocolRegistry = std::make_shared<llm::ProtocolRegistry>();
        
        // 注册各协议适配器
        m_protocolRegistry->registerAdapter(
            domain::model::ProviderType::OpenAIChatCompletionsCompatible,
            std::make_shared<llm::protocol::openai::OpenAIChatCompletionsAdapter>()
        );
        m_protocolRegistry->registerAdapter(
            domain::model::ProviderType::OpenAIResponses,
            std::make_shared<llm::protocol::openai_responses::OpenAIResponsesAdapter>()
        );
        m_protocolRegistry->registerAdapter(
            domain::model::ProviderType::Anthropic,
            std::make_shared<llm::protocol::anthropic::AnthropicProtocolAdapter>()
        );
        m_protocolRegistry->registerAdapter(
            domain::model::ProviderType::GoogleGemini,
            std::make_shared<llm::protocol::gemini::GeminiProtocolAdapter>()
        );
        m_protocolRegistry->registerAdapter(
            domain::model::ProviderType::Ollama,
            std::make_shared<llm::protocol::ollama::OllamaProtocolAdapter>()
        );

        m_chatGateway = std::make_unique<llm::ModelProviderService>(m_httpClient, m_protocolRegistry);

        // 2. 领域服务层初始化
        m_conversationService = std::make_unique<services::conversation::ConversationService>(m_conversationRepo.get());
        m_modelService = std::make_unique<services::model::ModelService>(m_modelRegistry);
        m_settingsService = std::make_unique<services::settings::SettingsService>();

        // 3. 对话业务用例初始化
        m_sendMessageUseCase = std::make_unique<application::usecase::chat::SendMessageUseCase>(
            m_chatGateway.get(),
            m_conversationService.get(),
            m_modelService.get()
        );
        m_stopGenerationUseCase = std::make_unique<application::usecase::chat::StopGenerationUseCase>(
            m_sendMessageUseCase.get()
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

        // 4. 工作流业务用例初始化
        m_startTaskUseCase = std::make_unique<application::usecase::work::StartTaskUseCase>();
        m_cancelTaskUseCase = std::make_unique<application::usecase::work::CancelTaskUseCase>();

        // 5. 知识库业务用例初始化
        m_searchDocumentsUseCase = std::make_unique<application::usecase::knowledge::SearchDocumentsUseCase>();
        m_addDocumentUseCase = std::make_unique<application::usecase::knowledge::AddDocumentUseCase>();

        // 6. 设置业务用例初始化
        m_loadSettingsUseCase = std::make_unique<application::usecase::settings::LoadSettingsUseCase>(m_settingsService.get());
        m_saveSettingUseCase = std::make_unique<application::usecase::settings::SaveSettingUseCase>(m_settingsService.get());
        m_getModelsUseCase = std::make_unique<application::usecase::settings::GetModelsUseCase>(m_modelService.get());

        // 7. ViewModels 表现层构造（直接注入对应域的 UseCase Bundle）
        m_mainViewModel = std::make_unique<ui::screen::main::MainViewModel>();
        m_chatViewModel = std::make_unique<ui::screen::chat::ChatViewModel>(chatUseCases());
        m_workViewModel = std::make_unique<ui::screen::work::WorkViewModel>(workUseCases());
        m_knowledgeViewModel = std::make_unique<ui::screen::knowledge::KnowledgeViewModel>(knowledgeUseCases());
        m_settingsViewModel = std::make_unique<ui::screen::settings::SettingsViewModel>(settingsUseCases());
    }

    ApplicationContext::~ApplicationContext() = default;

    data::sqlite::DatabaseManager &ApplicationContext::dbManager() {
        return data::sqlite::DatabaseManager::instance();
    }

    domain::repository::IConversationRepository *ApplicationContext::conversationRepository() const {
        return m_conversationRepo.get();
    }

    domain::repository::IModelRepository *ApplicationContext::modelRepository() const {
        return m_modelRepo.get();
    }

    core::model::ModelRegistry *ApplicationContext::modelRegistry() const {
        return m_modelRegistry.get();
    }

    domain::service::IConversationService *ApplicationContext::conversationService() const {
        return m_conversationService.get();
    }

    application::ports::IChatModelGateway *ApplicationContext::chatModelGateway() const {
        return m_chatGateway.get();
    }

    domain::service::IModelService *ApplicationContext::modelService() const {
        return m_modelService.get();
    }

    domain::service::ISettingsService *ApplicationContext::settingsService() const {
        return m_settingsService.get();
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

    application::usecase::work::WorkUseCases ApplicationContext::workUseCases() const {
        application::usecase::work::WorkUseCases w;
        w.startTask = m_startTaskUseCase.get();
        w.cancelTask = m_cancelTaskUseCase.get();
        return w;
    }

    application::usecase::knowledge::KnowledgeUseCases ApplicationContext::knowledgeUseCases() const {
        application::usecase::knowledge::KnowledgeUseCases k;
        k.searchDocuments = m_searchDocumentsUseCase.get();
        k.addDocument = m_addDocumentUseCase.get();
        return k;
    }

    application::usecase::settings::SettingsUseCases ApplicationContext::settingsUseCases() const {
        application::usecase::settings::SettingsUseCases s;
        s.loadSettings = m_loadSettingsUseCase.get();
        s.saveSetting = m_saveSettingUseCase.get();
        s.getModels = m_getModelsUseCase.get();
        return s;
    }

    ui::screen::main::MainViewModel *ApplicationContext::mainViewModel() const {
        return m_mainViewModel.get();
    }

    ui::screen::chat::ChatViewModel *ApplicationContext::chatViewModel() const {
        return m_chatViewModel.get();
    }

    ui::screen::work::WorkViewModel *ApplicationContext::workViewModel() const {
        return m_workViewModel.get();
    }

    ui::screen::knowledge::KnowledgeViewModel *ApplicationContext::knowledgeViewModel() const {
        return m_knowledgeViewModel.get();
    }

    ui::screen::settings::SettingsViewModel *ApplicationContext::settingsViewModel() const {
        return m_settingsViewModel.get();
    }
} // namespace app
