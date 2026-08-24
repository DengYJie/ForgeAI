cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(ForgeAI LANGUAGES C CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
        COMPONENTS
        Core
        Gui
        Widgets
        Network
        Sql
)

include(FetchContent)
find_package(Git REQUIRED)
FetchContent_Declare(
        fluentqt
        GIT_REPOSITORY https://github.com/L-1124/Fluent-Qt.git
        GIT_TAG my
)
FetchContent_MakeAvailable(fluentqt)

FetchContent_Declare(
        qwindowkit
        GIT_REPOSITORY https://github.com/stdware/qwindowkit.git
        GIT_TAG 1.5.0
)
FetchContent_MakeAvailable(qwindowkit)

set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(
        cmark-gfm
        GIT_REPOSITORY https://github.com/github/cmark-gfm.git
        GIT_TAG 0.29.0.gfm.13
)
FetchContent_GetProperties(cmark-gfm)
if(NOT cmark-gfm_POPULATED)
    FetchContent_Populate(cmark-gfm)
    add_subdirectory(${cmark-gfm_SOURCE_DIR} ${cmark-gfm_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

if (MSVC)
    if (TARGET libcmark-gfm_static)
        target_compile_options(libcmark-gfm_static PRIVATE /WX-)
    endif ()
    if (TARGET libcmark-gfm-extensions_static)
        target_compile_options(libcmark-gfm-extensions_static PRIVATE /WX-)
    endif ()
endif ()

if (TARGET libcmark-gfm_static AND NOT TARGET cmark-gfm::cmark-gfm)
    add_library(cmark-gfm::cmark-gfm ALIAS libcmark-gfm_static)
endif ()
if (TARGET libcmark-gfm-extensions_static AND NOT TARGET cmark-gfm::cmark-gfm-extensions)
    add_library(cmark-gfm::cmark-gfm-extensions ALIAS libcmark-gfm-extensions_static)
endif ()

qt_standard_project_setup()

set(PROJECT_SOURCES
        src/main.cpp
        
        src/core/logging/LogLevel.h
        src/core/logging/LogCategory.h
        src/core/logging/LogRecord.h
        src/core/logging/LogContext.h
        src/core/logging/SensitiveDataFilter.h
        src/core/logging/LogFormatter.h
        src/core/logging/sinks/ILogSink.h
        src/core/logging/sinks/ConsoleSink.h
        src/core/logging/sinks/ConsoleSink.cpp
        src/core/logging/sinks/RollingFileSink.h
        src/core/logging/sinks/RollingFileSink.cpp
        src/core/logging/ILogger.h
        src/core/logging/LoggingService.h
        src/core/logging/LoggingService.cpp
        src/core/logging/DiagnosticExporter.h
        src/core/logging/DiagnosticExporter.cpp
        src/core/logging/LoggingSettingsService.h
        src/core/logging/LoggingSettingsService.cpp

        src/network/HttpRequest.h
        src/network/HttpOperation.h
        src/network/IHttpClient.h
        src/network/QtHttpClient.h
        src/network/QtHttpClient.cpp
        
        src/llm/ProtocolRegistry.h
        src/llm/ProtocolRegistry.cpp
        src/llm/protocol/IStreamParser.h
        src/llm/protocol/IProtocolAdapter.h
        src/llm/protocol/openai/OpenAIStreamParser.h
        src/llm/protocol/openai/OpenAIStreamParser.cpp
        src/llm/protocol/openai/OpenAIChatCompletionsAdapter.h
        src/llm/protocol/openai/OpenAIChatCompletionsAdapter.cpp

        src/llm/protocol/openai_responses/OpenAIResponsesStreamParser.h
        src/llm/protocol/openai_responses/OpenAIResponsesStreamParser.cpp
        src/llm/protocol/openai_responses/OpenAIResponsesAdapter.h
        src/llm/protocol/openai_responses/OpenAIResponsesAdapter.cpp

        src/llm/protocol/anthropic/AnthropicStreamParser.h
        src/llm/protocol/anthropic/AnthropicStreamParser.cpp
        src/llm/protocol/anthropic/AnthropicProtocolAdapter.h
        src/llm/protocol/anthropic/AnthropicProtocolAdapter.cpp

        src/llm/protocol/gemini/GeminiStreamParser.h
        src/llm/protocol/gemini/GeminiStreamParser.cpp
        src/llm/protocol/gemini/GeminiProtocolAdapter.h
        src/llm/protocol/gemini/GeminiProtocolAdapter.cpp

        src/llm/protocol/ollama/OllamaStreamParser.h
        src/llm/protocol/ollama/OllamaStreamParser.cpp
        src/llm/protocol/ollama/OllamaProtocolAdapter.h
        src/llm/protocol/ollama/OllamaProtocolAdapter.cpp
        
        src/application/ports/IChatModelGateway.h
        src/application/ports/IModelDiscoveryGateway.h
        src/llm/runtime/RequestState.h
        src/llm/runtime/TimeoutPolicy.h
        src/llm/runtime/RetryPolicy.h
        src/llm/runtime/RetryDecision.h
        src/llm/runtime/RetryDecision.cpp
        src/llm/runtime/RequestMetrics.h
        src/llm/runtime/ChatOperation.h
        src/llm/runtime/ChatOperation.cpp
        src/llm/ModelProviderService.h
        src/llm/ModelProviderService.cpp
        src/llm/ModelDiscoveryService.h
        src/llm/ModelDiscoveryService.cpp
        src/ui/animation/AnimatedGlobalNavVisualSource.h
        src/ui/animation/AnimatedBackVisualSource.h
        src/ui/animation/AnimatedPanelLeftVisualSource.h
        src/ui/animation/AnimatedIcon.cpp
        src/ui/animation/AnimatedIcon.h
        src/ui/animation/AnimatedSettingsVisualSource.h
        src/ui/animation/AnimatedVisualSource.h

        src/ui/base/BaseViewModel.h
        src/ui/base/BasePage.h
        src/ui/base/BasePage.cpp

        src/ui/widget/CollapsibleSplitView.h
        src/ui/widget/CollapsibleSplitView.cpp
        src/ui/widget/MarkdownStyle.h
        src/ui/widget/MarkdownStyle.cpp
        src/ui/widget/MarkdownView.h
        src/ui/widget/MarkdownView.cpp

        src/ui/widget/chat/ChatHeader.h
        src/ui/widget/chat/ChatHeader.cpp
        src/ui/widget/chat/ChatAnchorBar.h
        src/ui/widget/chat/ChatAnchorBar.cpp
        src/ui/widget/chat/ChatInputBox.h
        src/ui/widget/chat/ChatInputBox.cpp

        src/ui/widget/message/MessageCardWidget.h
        src/ui/widget/message/MessageCardWidget.cpp
        src/ui/widget/message/MessageListView.h
        src/ui/widget/message/MessageListView.cpp
        src/ui/widget/message/ProcessGroupWidget.h
        src/ui/widget/message/ProcessGroupWidget.cpp
        src/ui/widget/message/blocks/ThinkingBlockWidget.h
        src/ui/widget/message/blocks/ThinkingBlockWidget.cpp
        src/ui/widget/message/blocks/AbstractToolBlockWidget.h
        src/ui/widget/message/blocks/AbstractToolBlockWidget.cpp
        src/ui/widget/message/blocks/ToolWidgetFactory.h
        src/ui/widget/message/blocks/ToolWidgetFactory.cpp
        src/ui/widget/message/blocks/tools/BashToolWidget.h
        src/ui/widget/message/blocks/tools/BashToolWidget.cpp
        src/ui/widget/message/blocks/tools/FileReadToolWidget.h
        src/ui/widget/message/blocks/tools/FileReadToolWidget.cpp
        src/ui/widget/message/blocks/tools/FileDiffToolWidget.h
        src/ui/widget/message/blocks/tools/FileDiffToolWidget.cpp
        src/ui/widget/message/blocks/tools/GenericToolWidget.h
        src/ui/widget/message/blocks/tools/GenericToolWidget.cpp
        src/ui/widget/message/blocks/ImageBlockWidget.h
        src/ui/widget/message/blocks/ImageBlockWidget.cpp
        src/ui/widget/message/blocks/ErrorBlockWidget.h
        src/ui/widget/message/blocks/ErrorBlockWidget.cpp
        src/ui/widget/message/blocks/FlatExpander.h
        src/ui/widget/message/blocks/FlatExpander.cpp

        src/ui/navigation/NavigationWidget.h
        src/ui/navigation/NavigationWidget.cpp
        src/ui/navigation/NavigationToolButton.h
        src/ui/navigation/NavigationToolButton.cpp
        src/ui/navigation/NavigationSeparator.h
        src/ui/navigation/NavigationSeparator.cpp
        src/ui/navigation/NavigationPushButton.h
        src/ui/navigation/NavigationPushButton.cpp
        src/ui/navigation/NavigationTreeItem.h
        src/ui/navigation/NavigationTreeItem.cpp
        src/ui/navigation/NavigationSectionHeader.h
        src/ui/navigation/NavigationSectionHeader.cpp
        src/ui/navigation/NavigationIndicator.h
        src/ui/navigation/NavigationIndicator.cpp
        src/ui/navigation/NavigationTreeWidget.h
        src/ui/navigation/NavigationTreeWidget.cpp
        src/ui/navigation/NavigationTreeWidgetBase.h
        src/ui/navigation/NavigationTreeWidgetBase.cpp
        src/ui/navigation/NavigationFocusHost.h
        src/ui/navigation/NavigationMetrics.h
        src/ui/navigation/NavigationFlyout.h
        src/ui/navigation/NavigationFlyout.cpp
        src/ui/navigation/NavigationPanel.h
        src/ui/navigation/NavigationPanel.cpp
        src/ui/navigation/NavigationHistory.h
        src/ui/navigation/NavigationHistory.cpp
        src/ui/navigation/NavigationTypes.h
        src/ui/navigation/INavigationPresenter.h
        src/ui/navigation/INavigationRegistrar.h
        src/ui/navigation/NavigationController.h
        src/ui/navigation/NavigationController.cpp

        src/ui/window/TitleBar.cpp
        src/ui/window/TitleBar.h
        src/ui/window/WindowBase.h
        src/ui/window/WindowBase.cpp
        src/ui/window/NavigationWindow.h
        src/ui/window/NavigationWindow.cpp

        src/ui/screen/main/MainViewModel.h
        src/ui/screen/main/MainViewModel.cpp
        src/ui/screen/main/MainWindow.h
        src/ui/screen/main/MainWindow.cpp

        src/ui/screen/chat/ChatViewModel.h
        src/ui/screen/chat/ChatViewModel.cpp
        src/ui/screen/chat/ChatSessionListModel.h
        src/ui/screen/chat/ChatSessionListModel.cpp
        src/ui/screen/chat/ChatSessionDelegate.h
        src/ui/screen/chat/ChatSessionDelegate.cpp
        src/ui/screen/chat/ChatSidebar.h
        src/ui/screen/chat/ChatSidebar.cpp
        src/ui/screen/chat/ChatPage.h
        src/ui/screen/chat/ChatPage.cpp

        src/ui/screen/work/WorkViewModel.h
        src/ui/screen/work/WorkViewModel.cpp
        src/ui/screen/work/WorkPage.h
        src/ui/screen/work/WorkPage.cpp

        src/ui/screen/knowledge/KnowledgeViewModel.h
        src/ui/screen/knowledge/KnowledgeViewModel.cpp
        src/ui/screen/knowledge/KnowledgePage.h
        src/ui/screen/knowledge/KnowledgePage.cpp

        src/core/settings/SettingsRegistry.h
        src/core/settings/SettingsRegistry.cpp
        src/core/settings/ISettingsProvider.h
        src/core/settings/BaseSettingsProvider.h
        src/core/settings/providers/AppearanceSettingsProvider.h
        src/core/settings/providers/AppearanceSettingsProvider.cpp
        src/core/settings/providers/LoggingSettingsProvider.h
        src/core/settings/providers/LoggingSettingsProvider.cpp
        src/core/settings/providers/ModelSettingsProvider.h
        src/core/settings/providers/ModelSettingsProvider.cpp
        src/ui/screen/settings/ISettingsUIFactory.h
        src/ui/screen/settings/SettingsUIRegistry.h
        src/ui/screen/settings/SettingsUIRegistry.cpp
        src/ui/screen/settings/appearance/AppearanceSettingsViewModel.h
        src/ui/screen/settings/appearance/AppearanceSettingsViewModel.cpp
        src/ui/screen/settings/appearance/AppearanceSettingsUIFactory.h
        src/ui/screen/settings/appearance/AppearanceSettingsUIFactory.cpp
        src/ui/screen/settings/logging/LoggingSettingsViewModel.h
        src/ui/screen/settings/logging/LoggingSettingsViewModel.cpp
        src/ui/screen/settings/logging/LoggingSettingsUIFactory.h
        src/ui/screen/settings/logging/LoggingSettingsUIFactory.cpp
        src/ui/screen/settings/model/ModelSettingsPageFactory.h
        src/ui/screen/settings/model/ModelSettingsPageFactory.cpp
        src/ui/screen/settings/model_manager/ModelItemCard.h
        src/ui/screen/settings/model_manager/ModelItemCard.cpp
        src/ui/screen/settings/model_manager/ProviderListModel.h
        src/ui/screen/settings/model_manager/ProviderListModel.cpp
        src/ui/screen/settings/model_manager/ProviderFilterProxyModel.h
        src/ui/screen/settings/model_manager/ProviderFilterProxyModel.cpp
        src/ui/screen/settings/model_manager/ProviderItemDelegate.h
        src/ui/screen/settings/model_manager/ProviderItemDelegate.cpp
        src/ui/screen/settings/model_manager/ProviderNavigationPane.h
        src/ui/screen/settings/model_manager/ProviderNavigationPane.cpp
        src/ui/screen/settings/model_manager/ProviderDetailView.h
        src/ui/screen/settings/model_manager/ProviderDetailView.cpp
        src/ui/screen/settings/model_manager/AddProviderDialog.h
        src/ui/screen/settings/model_manager/AddProviderDialog.cpp
        src/ui/screen/settings/model_manager/AddModelDialog.h
        src/ui/screen/settings/model_manager/AddModelDialog.cpp
        src/ui/screen/settings/model_manager/ModelActionsSplitButton.h
        src/ui/screen/settings/model_manager/ModelActionsSplitButton.cpp
        src/ui/screen/settings/model_manager/ModelManagerViewModel.h
        src/ui/screen/settings/model_manager/ModelManagerViewModel.cpp
        src/ui/screen/settings/model_manager/ModelManagerPage.h
        src/ui/screen/settings/model_manager/ModelManagerPage.cpp
        src/ui/screen/settings/SettingsDescriptorBuilder.h
        src/ui/screen/settings/SettingsDescriptorBuilder.cpp
        src/ui/screen/settings/SettingsPageFactory.h
        src/ui/screen/settings/SettingsPageFactory.cpp
        src/ui/screen/settings/SettingsNavigationModule.h
        src/ui/screen/settings/SettingsNavigationModule.cpp

        src/domain/Types.h
        src/domain/conversation/Conversation.h
        src/domain/conversation/MessageBlock.h
        src/domain/conversation/Message.h
        src/domain/agent/Agent.h
        src/domain/agent/ToolDefinition.h
        src/domain/agent/ToolExecution.h
        src/domain/agent/RuntimeCheckpoint.h
        src/domain/project/Project.h
        src/domain/model/ModelCapabilities.h
        src/domain/model/ModelParameters.h
        src/domain/model/Model.h
        src/domain/model/ModelProvider.h
        src/domain/llm/ChatError.h
        src/domain/llm/ChatMessage.h
        src/domain/llm/ChatRequest.h
        src/domain/llm/ChatEvent.h
        src/domain/repository/IConversationRepository.h
        src/domain/repository/IMessageTranscriptRepository.h
        src/domain/repository/IProjectRepository.h
        src/domain/repository/IModelRepository.h

        src/data/sqlite/SqlTransaction.h
        src/data/sqlite/SqlTransaction.cpp
        src/data/sqlite/SqlHelper.h
        src/data/sqlite/SqlHelper.cpp
        src/data/sqlite/DatabaseManager.h
        src/data/sqlite/DatabaseManager.cpp
        src/data/repository/SqliteConversationRepository.h
        src/data/repository/SqliteConversationRepository.cpp
        src/data/repository/SqliteProjectRepository.h
        src/data/repository/SqliteModelRepository.h
        src/data/repository/SqliteModelRepository.cpp
        src/data/repository/JsonlMessageRepository.h
        src/data/repository/JsonlMessageRepository.cpp
        src/data/importer/ModelsDevImporter.h
        src/data/importer/ModelsDevImporter.cpp

        src/core/context/ContextManager.h
        src/core/context/ContextManager.cpp
        src/core/context/PromptBuilder.h
        src/core/context/PromptBuilder.cpp
        src/core/model/ModelRegistry.h
        src/core/model/ModelRegistry.cpp

        src/domain/service/IConversationService.h
        src/domain/service/IModelService.h
        src/domain/service/ISettingsService.h

        src/services/conversation/ConversationService.h
        src/services/conversation/ConversationService.cpp
        src/services/model/ModelService.h
        src/services/model/ModelService.cpp
        src/services/settings/SettingsService.h
        src/services/settings/SettingsService.cpp

        src/application/usecase/chat/ChatUseCases.h
        src/application/usecase/chat/SendMessageUseCase.h
        src/application/usecase/chat/SendMessageUseCase.cpp
        src/application/usecase/chat/StopGenerationUseCase.h
        src/application/usecase/chat/StopGenerationUseCase.cpp
        src/application/usecase/conversation/LoadSessionsUseCase.h
        src/application/usecase/conversation/LoadSessionsUseCase.cpp
        src/application/usecase/conversation/LoadSessionDetailUseCase.h
        src/application/usecase/conversation/LoadSessionDetailUseCase.cpp
        src/application/usecase/conversation/CreateSessionUseCase.h
        src/application/usecase/conversation/CreateSessionUseCase.cpp
        src/application/usecase/conversation/DeleteSessionUseCase.h
        src/application/usecase/conversation/DeleteSessionUseCase.cpp

        src/application/usecase/work/WorkUseCases.h
        src/application/usecase/work/StartTaskUseCase.h
        src/application/usecase/work/StartTaskUseCase.cpp
        src/application/usecase/work/CancelTaskUseCase.h
        src/application/usecase/work/CancelTaskUseCase.cpp

        src/application/usecase/knowledge/KnowledgeUseCases.h
        src/application/usecase/knowledge/SearchDocumentsUseCase.h
        src/application/usecase/knowledge/SearchDocumentsUseCase.cpp
        src/application/usecase/knowledge/AddDocumentUseCase.h
        src/application/usecase/knowledge/AddDocumentUseCase.cpp

        src/application/usecase/settings/SettingsUseCases.h
        src/application/usecase/settings/LoadSettingsUseCase.h
        src/application/usecase/settings/LoadSettingsUseCase.cpp
        src/application/usecase/settings/SaveSettingUseCase.h
        src/application/usecase/settings/SaveSettingUseCase.cpp
        src/application/usecase/settings/GetSettingsProvidersUseCase.h
        src/application/usecase/settings/GetSettingsProvidersUseCase.cpp
        src/application/usecase/settings/GetModelsUseCase.h
        src/application/usecase/settings/GetModelsUseCase.cpp
        src/application/usecase/settings/RefreshModelsUseCase.h
        src/application/usecase/settings/RefreshModelsUseCase.cpp
        src/application/usecase/settings/SaveProviderUseCase.h
        src/application/usecase/settings/SaveProviderUseCase.cpp
        src/application/usecase/settings/DeleteProviderUseCase.h
        src/application/usecase/settings/DeleteProviderUseCase.cpp

        src/app/ApplicationContext.h
        src/app/ApplicationContext.cpp
        src/app/Application.h
        src/app/Application.cpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

add_subdirectory(res)

set_target_properties(${PROJECT_NAME}
        PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        CMARK_GFM_STATIC_DEFINE
        CMARK_GFM_EXTENSIONS_STATIC_DEFINE
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/extensions
        ${cmark-gfm_BINARY_DIR}/src
        ${cmark-gfm_BINARY_DIR}/extensions
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::Sql
        QWindowKit::Widgets
        FluentQt::FluentQt
        cmark-gfm::cmark-gfm
        cmark-gfm::cmark-gfm-extensions
)

if (WIN32)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
                COMMAND_EXPAND_LISTS
                COMMENT "Copying required third-party runtime DLLs..."
        )
    endif ()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Deploying Qt libraries..."
            COMMAND "${CMAKE_PREFIX_PATH}/bin/windeployqt.exe"
            --verbose 0
            --no-compiler-runtime
            --no-opengl-sw
            "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Running windeployqt to copy dependencies..."
    )
endif ()

