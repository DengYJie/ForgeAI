#pragma once

#include <memory>
#include <QObject>
#include "data/sqlite/DatabaseManager.h"
#include "data/repository/SqliteConversationRepository.h"
#include "data/repository/SqliteModelRepository.h"
#include "core/model/ModelRegistry.h"
#include "core/settings/SettingsRegistry.h"
#include "core/settings/providers/AppearanceSettingsProvider.h"
#include "core/settings/providers/LoggingSettingsProvider.h"
#include "core/settings/providers/ModelSettingsProvider.h"
#include "network/QtHttpClient.h"
#include "llm/ProtocolRegistry.h"
#include "llm/ModelProviderService.h"
#include "llm/ModelDiscoveryService.h"
#include "services/conversation/ConversationService.h"
#include "services/model/ModelService.h"
#include "services/settings/SettingsService.h"
#include "application/usecase/chat/ChatUseCases.h"
#include "application/usecase/work/WorkUseCases.h"
#include "application/usecase/knowledge/KnowledgeUseCases.h"
#include "application/usecase/settings/SettingsUseCases.h"
#include "application/usecase/settings/GetSettingsProvidersUseCase.h"
#include "application/usecase/settings/SaveProviderUseCase.h"
#include "application/usecase/settings/DeleteProviderUseCase.h"
#include "application/usecase/settings/TestProviderConnectionUseCase.h"
#include "ui/screen/main/MainViewModel.h"
#include "ui/screen/chat/ChatViewModel.h"
#include "ui/screen/work/WorkViewModel.h"
#include "ui/screen/knowledge/KnowledgeViewModel.h"
#include "ui/screen/settings/SettingsUIRegistry.h"
#include "ui/screen/settings/appearance/AppearanceSettingsViewModel.h"
#include "ui/screen/settings/logging/LoggingSettingsViewModel.h"
#include "ui/screen/settings/model_manager/ModelManagerViewModel.h"

namespace app {
    /**
     * @brief 应用程序级组合根 (Composition Root)
     * @details 在 main 栈上统一构造与编排全量基础设施、仓储、服务、业务用例与 ViewModels，
     *          通过构造函数注入传递给 Views，实现全局完整的 Clean Architecture。
     */
    class ApplicationContext {
    public:
        ApplicationContext();
        ~ApplicationContext();

        // 1. 基础设施层
        data::sqlite::DatabaseManager &dbManager();

        // 2. 仓储层
        domain::repository::IConversationRepository *conversationRepository() const;
        domain::repository::IModelRepository *modelRepository() const;

        // 3. 全局核心注册中心
        core::model::ModelRegistry *modelRegistry() const;
        core::settings::SettingsRegistry *settingsRegistry() const;
        ui::screen::settings::SettingsUIRegistry *settingsUiRegistry() const;

        // 4. 服务层
        domain::service::IConversationService *conversationService() const;
        domain::service::IModelService *modelService() const;
        domain::service::ISettingsService *settingsService() const;
        application::ports::IChatModelGateway *chatModelGateway() const;

        // 5. UseCase 聚合包
        application::usecase::chat::ChatUseCases chatUseCases() const;
        application::usecase::work::WorkUseCases workUseCases() const;
        application::usecase::knowledge::KnowledgeUseCases knowledgeUseCases() const;
        application::usecase::settings::SettingsUseCases settingsUseCases() const;

        // 6. ViewModels 表现层
        ui::screen::main::MainViewModel *mainViewModel() const;
        ui::screen::chat::ChatViewModel *chatViewModel() const;
        ui::screen::work::WorkViewModel *workViewModel() const;
        ui::screen::knowledge::KnowledgeViewModel *knowledgeViewModel() const;

        ui::screen::settings::AppearanceSettingsViewModel *appearanceSettingsViewModel() const;
        ui::screen::settings::LoggingSettingsViewModel *loggingSettingsViewModel() const;
        ui::screen::settings::model_manager::ModelManagerViewModel *modelManagerViewModel() const;

    private:
        void registerSettings();

        // 仓储与基础组件
        std::unique_ptr<data::repository::SqliteConversationRepository> m_conversationRepo;
        std::shared_ptr<data::repository::SqliteModelRepository> m_modelRepo;
        std::shared_ptr<core::model::ModelRegistry> m_modelRegistry;

        // 设置持久化与提供者
        std::unique_ptr<core::settings::SettingsRegistry> m_settingsRegistry;
        std::shared_ptr<core::settings::AppearanceSettingsProvider> m_appearanceSettingsProvider;
        std::shared_ptr<core::settings::LoggingSettingsProvider> m_loggingSettingsProvider;
        std::shared_ptr<core::settings::ModelSettingsProvider> m_modelSettingsProvider;

        // 网络与 LLM 网关
        std::shared_ptr<network::QtHttpClient> m_httpClient;
        std::shared_ptr<llm::ProtocolRegistry> m_protocolRegistry;
        std::unique_ptr<llm::ModelProviderService> m_chatGateway;
        std::unique_ptr<llm::ModelDiscoveryService> m_discoveryGateway;

        // 领域服务
        std::unique_ptr<services::conversation::ConversationService> m_conversationService;
        std::unique_ptr<services::model::ModelService> m_modelService;
        std::unique_ptr<services::settings::SettingsService> m_settingsService;

        // 对话业务用例
        std::unique_ptr<application::usecase::chat::SendMessageUseCase> m_sendMessageUseCase;
        std::unique_ptr<application::usecase::chat::StopGenerationUseCase> m_stopGenerationUseCase;
        std::unique_ptr<application::usecase::conversation::LoadSessionsUseCase> m_loadSessionsUseCase;
        std::unique_ptr<application::usecase::conversation::LoadSessionDetailUseCase> m_loadSessionDetailUseCase;
        std::unique_ptr<application::usecase::conversation::CreateSessionUseCase> m_createSessionUseCase;
        std::unique_ptr<application::usecase::conversation::DeleteSessionUseCase> m_deleteSessionUseCase;

        // 工作流业务用例
        std::unique_ptr<application::usecase::work::StartTaskUseCase> m_startTaskUseCase;
        std::unique_ptr<application::usecase::work::CancelTaskUseCase> m_cancelTaskUseCase;

        // 知识库业务用例
        std::unique_ptr<application::usecase::knowledge::SearchDocumentsUseCase> m_searchDocumentsUseCase;
        std::unique_ptr<application::usecase::knowledge::AddDocumentUseCase> m_addDocumentUseCase;

        // 设置业务用例
        std::unique_ptr<application::usecase::settings::LoadSettingsUseCase> m_loadSettingsUseCase;
        std::unique_ptr<application::usecase::settings::SaveSettingUseCase> m_saveSettingUseCase;
        std::unique_ptr<application::usecase::settings::GetSettingsProvidersUseCase> m_getSettingsProvidersUseCase;
        std::unique_ptr<application::usecase::settings::GetModelsUseCase> m_getModelsUseCase;
        std::unique_ptr<application::usecase::settings::RefreshModelsUseCase> m_refreshModelsUseCase;
        std::unique_ptr<application::usecase::settings::SaveProviderUseCase> m_saveProviderUseCase;
        std::unique_ptr<application::usecase::settings::DeleteProviderUseCase> m_deleteProviderUseCase;
        std::unique_ptr<application::usecase::settings::TestProviderConnectionUseCase> m_testProviderConnectionUseCase;

        // 全局 ViewModels
        std::unique_ptr<ui::screen::main::MainViewModel> m_mainViewModel;
        std::unique_ptr<ui::screen::chat::ChatViewModel> m_chatViewModel;
        std::unique_ptr<ui::screen::work::WorkViewModel> m_workViewModel;
        std::unique_ptr<ui::screen::knowledge::KnowledgeViewModel> m_knowledgeViewModel;

        // 设置体系 ViewModels & UI Registry
        std::unique_ptr<ui::screen::settings::AppearanceSettingsViewModel> m_appearanceSettingsViewModel;
        std::unique_ptr<ui::screen::settings::LoggingSettingsViewModel> m_loggingSettingsViewModel;
        std::unique_ptr<ui::screen::settings::model_manager::ModelManagerViewModel> m_modelManagerViewModel;
        std::unique_ptr<ui::screen::settings::SettingsUIRegistry> m_settingsUiRegistry;
    };
} // namespace app
