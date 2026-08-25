#include "ApplicationContext.h"
#include <QDir>
#include "core/logging/LoggingService.h"
#include "core/logging/sinks/ConsoleSink.h"
#include "core/logging/sinks/RollingFileSink.h"
#include "core/logging/LogCategory.h"
#include "llm/protocol/openai/OpenAIChatCompletionsAdapter.h"
#include "llm/protocol/anthropic/AnthropicProtocolAdapter.h"
#include "llm/protocol/gemini/GeminiProtocolAdapter.h"
#include "llm/protocol/ollama/OllamaProtocolAdapter.h"
#include "llm/protocol/openai_responses/OpenAIResponsesAdapter.h"
#include "ui/screen/settings/appearance/AppearanceSettingsUIFactory.h"
#include "ui/screen/settings/logging/LoggingSettingsUIFactory.h"
#include "ui/screen/settings/model/ModelSettingsPageFactory.h"
#include <QSysInfo>
#include <QUuid>

namespace app {
    ApplicationContext::ApplicationContext() {
        auto &logger = core::logging::LoggingService::instance();
        logger.addSink(std::make_shared<core::logging::ConsoleSink>(true));
        logger.addSink(std::make_shared<core::logging::RollingFileSink>());
        logger.installQtMessageHandler();

        QString appSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        logger.info(core::logging::Category::AppLifecycle, QStringLiteral("ForgeAI starting..."), {
            {QStringLiteral("sessionId"), appSessionId},
            {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
            {QStringLiteral("os"), QSysInfo::prettyProductName()},
            {QStringLiteral("arch"), QSysInfo::currentCpuArchitecture()}
        });

        // 1. 仓储与基础组件初始化
        m_conversationRepo = std::make_unique<data::repository::SqliteConversationRepository>();
        m_messageTranscriptRepo = std::make_unique<data::repository::JsonlMessageRepository>(
            QDir::homePath() + QStringLiteral("/.forgeai/sessions"));
        m_modelRepo = std::make_shared<data::repository::SqliteModelRepository>();
        m_projectRepo = std::make_unique<data::repository::SqliteProjectRepository>();
        m_agentRepo = std::make_unique<data::repository::SqliteAgentRepository>();
        m_agentCheckpointRepo = std::make_unique<data::repository::SqliteAgentCheckpointRepository>();
        m_modelRegistry = std::make_shared<core::model::ModelRegistry>(m_modelRepo);

        // 1.5 设置系统持久化与 Providers 初始化
        m_settingsRegistry = std::make_unique<core::settings::SettingsRegistry>();
        m_appearanceSettingsProvider = std::make_shared<core::settings::AppearanceSettingsProvider>();
        m_loggingSettingsProvider = std::make_shared<core::settings::LoggingSettingsProvider>();
        m_modelSettingsProvider = std::make_shared<core::settings::ModelSettingsProvider>();

        m_settingsRegistry->registerProvider(m_appearanceSettingsProvider);
        m_settingsRegistry->registerProvider(m_loggingSettingsProvider);
        m_settingsRegistry->registerProvider(m_modelSettingsProvider);

        // 1.8 网络与 LLM 协议注册
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
        m_discoveryGateway = std::make_unique<llm::ModelDiscoveryService>(m_httpClient, m_protocolRegistry);

        // 2. 领域服务与工具体系初始化
        m_workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        m_processTaskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
        m_builtinToolProvider = std::make_shared<agent::tool::BuiltinToolProvider>(m_processTaskRuntime, m_workspaceFs);
        m_mcpManager = std::make_unique<llm::mcp::McpManager>();
        m_toolRegistry = std::make_unique<agent::tool::ToolRegistry>();
        m_toolRegistry->registerProvider(m_builtinToolProvider);
        m_toolRegistry->registerProvider(m_mcpManager->toolProvider());
        m_skillRegistry = std::make_unique<agent::skill::SkillRegistry>();

        m_conversationService = std::make_unique<services::conversation::ConversationService>(
            m_conversationRepo.get(), m_messageTranscriptRepo.get());
        m_modelService = std::make_unique<services::model::ModelService>(m_modelRegistry);
        m_settingsService = std::make_unique<services::settings::SettingsService>(m_settingsRegistry.get());
        m_projectContextService = std::make_unique<services::project::ProjectContextService>();

        // 2.5 Agent 运行时与 UseCases 初始化
        m_agentRuntime = std::make_unique<agent::runtime::AgentRuntime>(
            m_chatGateway.get(),
            m_conversationService.get(),
            m_toolRegistry.get(),
            m_agentCheckpointRepo.get(),
            m_processTaskRuntime
        );
        m_mcpProjectRuntimeCoordinator = std::make_unique<llm::mcp::McpProjectRuntimeCoordinator>(
            m_mcpManager.get()
        );
        m_runAgentUseCase = std::make_unique<application::usecase::agent::RunAgentUseCase>(
            m_agentRuntime.get(),
            m_modelService.get(),
            m_projectContextService.get(),
            m_mcpProjectRuntimeCoordinator.get(),
            m_agentRepo.get(),
            m_skillRegistry.get()
        );
        m_cancelAgentRunUseCase = std::make_unique<application::usecase::agent::CancelAgentRunUseCase>(
            m_agentRuntime.get()
        );
        m_resumeAgentRunUseCase = std::make_unique<application::usecase::agent::ResumeAgentRunUseCase>(
            m_agentRuntime.get()
        );
        m_switchProjectUseCase = std::make_unique<application::usecase::work::SwitchProjectUseCase>(
            m_mcpProjectRuntimeCoordinator.get(),
            m_projectContextService.get()
        );

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
        m_clearSessionUseCase = std::make_unique<application::usecase::conversation::ClearSessionUseCase>(
            m_conversationService.get()
        );
        m_setSessionPinnedUseCase = std::make_unique<application::usecase::conversation::SetSessionPinnedUseCase>(
            m_conversationService.get()
        );
        m_setSessionArchivedUseCase = std::make_unique<application::usecase::conversation::SetSessionArchivedUseCase>(
            m_conversationService.get()
        );
        m_setSessionTitleUseCase = std::make_unique<application::usecase::conversation::SetSessionTitleUseCase>(
            m_conversationService.get()
        );

        // 4. 知识库业务用例初始化
        m_searchDocumentsUseCase = std::make_unique<application::usecase::knowledge::SearchDocumentsUseCase>();
        m_addDocumentUseCase = std::make_unique<application::usecase::knowledge::AddDocumentUseCase>();

        // 5. 设置业务用例初始化
        m_loadSettingsUseCase = std::make_unique<application::usecase::settings::LoadSettingsUseCase>(m_settingsService.get());
        m_saveSettingUseCase = std::make_unique<application::usecase::settings::SaveSettingUseCase>(m_settingsService.get());
        m_getSettingsProvidersUseCase = std::make_unique<application::usecase::settings::GetSettingsProvidersUseCase>(m_settingsService.get());
        m_getModelsUseCase = std::make_unique<application::usecase::settings::GetModelsUseCase>(m_modelService.get());
        m_refreshModelsUseCase = std::make_unique<application::usecase::settings::RefreshModelsUseCase>(
            m_discoveryGateway.get(),
            m_modelRegistry
        );
        m_saveProviderUseCase = std::make_unique<application::usecase::settings::SaveProviderUseCase>(m_modelService.get());
        m_deleteProviderUseCase = std::make_unique<application::usecase::settings::DeleteProviderUseCase>(m_modelService.get());
        m_testProviderConnectionUseCase = std::make_unique<application::usecase::settings::TestProviderConnectionUseCase>(m_discoveryGateway.get());

        // 7. ViewModels 表现层构造
        m_mainViewModel = std::make_unique<ui::screen::main::MainViewModel>();
        m_chatViewModel = std::make_unique<ui::screen::chat::ChatViewModel>(chatUseCases());
        m_workViewModel = std::make_unique<ui::screen::work::WorkViewModel>(workUseCases(), m_projectContextService.get());
        m_knowledgeViewModel = std::make_unique<ui::screen::knowledge::KnowledgeViewModel>(knowledgeUseCases());

        // 7.5 设置系统局部 ViewModels 与页面 ViewModel
        m_appearanceSettingsViewModel = std::make_unique<ui::screen::settings::AppearanceSettingsViewModel>(m_appearanceSettingsProvider.get());
        m_loggingSettingsViewModel = std::make_unique<ui::screen::settings::LoggingSettingsViewModel>(m_loggingSettingsProvider.get());
        m_modelManagerViewModel = std::make_unique<ui::screen::settings::model_manager::ModelManagerViewModel>(
            m_getModelsUseCase.get(),
            m_saveProviderUseCase.get(),
            m_deleteProviderUseCase.get(),
            m_refreshModelsUseCase.get(),
            m_testProviderConnectionUseCase.get()
        );
        // 8. 显式 DI 注册所有 Settings UIFactories
        registerSettings();
    }

    ApplicationContext::~ApplicationContext() {
        auto &logger = core::logging::LoggingService::instance();
        logger.info(core::logging::Category::AppLifecycle, QStringLiteral("ForgeAI shutting down..."));
        logger.flush();
    }

    void ApplicationContext::registerSettings() {
        m_settingsUiRegistry = std::make_unique<ui::screen::settings::SettingsUIRegistry>();

        m_settingsUiRegistry->registerProviderPageFactory(
            std::make_shared<ui::screen::settings::ModelSettingsPageFactory>(
                m_modelManagerViewModel.get())
        );

        m_settingsUiRegistry->registerFactory(
            std::make_shared<ui::screen::settings::AppearanceSettingsUIFactory>(m_appearanceSettingsViewModel.get())
        );

        m_settingsUiRegistry->registerFactory(
            std::make_shared<ui::screen::settings::LoggingLevelSettingsUIFactory>(m_loggingSettingsViewModel.get())
        );
        m_settingsUiRegistry->registerFactory(
            std::make_shared<ui::screen::settings::LoggingStorageSettingsUIFactory>(m_loggingSettingsViewModel.get())
        );
        m_settingsUiRegistry->registerFactory(
            std::make_shared<ui::screen::settings::LoggingOpenDirSettingsUIFactory>(m_loggingSettingsViewModel.get())
        );
        m_settingsUiRegistry->registerFactory(
            std::make_shared<ui::screen::settings::LoggingExportSettingsUIFactory>(m_loggingSettingsViewModel.get())
        );
    }

    data::sqlite::DatabaseManager &ApplicationContext::dbManager() {
        return data::sqlite::DatabaseManager::instance();
    }

    domain::repository::IConversationRepository *ApplicationContext::conversationRepository() const {
        return m_conversationRepo.get();
    }

    domain::repository::IModelRepository *ApplicationContext::modelRepository() const {
        return m_modelRepo.get();
    }

    domain::repository::IProjectRepository *ApplicationContext::projectRepository() const {
        return m_projectRepo.get();
    }

    core::model::ModelRegistry *ApplicationContext::modelRegistry() const {
        return m_modelRegistry.get();
    }

    core::settings::SettingsRegistry *ApplicationContext::settingsRegistry() const {
        return m_settingsRegistry.get();
    }

    ui::screen::settings::SettingsUIRegistry *ApplicationContext::settingsUiRegistry() const {
        return m_settingsUiRegistry.get();
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

    agent::tool::ToolRegistry *ApplicationContext::toolRegistry() const {
        return m_toolRegistry.get();
    }

    agent::skill::SkillRegistry *ApplicationContext::skillRegistry() const {
        return m_skillRegistry.get();
    }

    application::ports::IAgentRuntime *ApplicationContext::agentRuntime() const {
        return m_agentRuntime.get();
    }

    application::usecase::chat::ChatUseCases ApplicationContext::chatUseCases() const {
        application::usecase::chat::ChatUseCases c;
        c.sendMessage = m_sendMessageUseCase.get();
        c.stopGeneration = m_stopGenerationUseCase.get();
        c.loadSessions = m_loadSessionsUseCase.get();
        c.loadSessionDetail = m_loadSessionDetailUseCase.get();
        c.createSession = m_createSessionUseCase.get();
        c.deleteSession = m_deleteSessionUseCase.get();
        c.clearSession = m_clearSessionUseCase.get();
        c.setSessionPinned = m_setSessionPinnedUseCase.get();
        c.setSessionArchived = m_setSessionArchivedUseCase.get();
        c.setSessionTitle = m_setSessionTitleUseCase.get();
        c.getModels = m_getModelsUseCase.get();
        return c;
    }

    application::usecase::work::WorkUseCases ApplicationContext::workUseCases() const {
        application::usecase::work::WorkUseCases w;
        w.runAgent = m_runAgentUseCase.get();
        w.cancelAgentRun = m_cancelAgentRunUseCase.get();
        w.conversationService = m_conversationService.get();
        w.conversationRepository = m_conversationRepo.get();
        w.projectRepository = m_projectRepo.get();
        w.switchProject = m_switchProjectUseCase.get();
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
        s.getSettingsProviders = m_getSettingsProvidersUseCase.get();
        s.getModels = m_getModelsUseCase.get();
        s.refreshModels = m_refreshModelsUseCase.get();
        s.saveProvider = m_saveProviderUseCase.get();
        s.deleteProvider = m_deleteProviderUseCase.get();
        s.modelRegistry = m_modelRegistry;
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

    ui::screen::settings::AppearanceSettingsViewModel *ApplicationContext::appearanceSettingsViewModel() const {
        return m_appearanceSettingsViewModel.get();
    }

    ui::screen::settings::LoggingSettingsViewModel *ApplicationContext::loggingSettingsViewModel() const {
        return m_loggingSettingsViewModel.get();
    }

    ui::screen::settings::model_manager::ModelManagerViewModel *ApplicationContext::modelManagerViewModel() const {
        return m_modelManagerViewModel.get();
    }
} // namespace app
