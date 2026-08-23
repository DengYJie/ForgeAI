cmake_minimum_required(VERSION 3.16)
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

FetchContent_Declare(
        md4c
        GIT_REPOSITORY https://github.com/mity/md4c.git
        GIT_TAG release-0.5.2
)
set(MD4C_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(md4c)
FetchContent_GetProperties(md4c SOURCE_DIR md4c_SOURCE_DIR)

FetchContent_Declare(
        litehtml
        GIT_REPOSITORY https://github.com/litehtml/litehtml.git
        GIT_TAG v0.10
        PATCH_COMMAND
        ${GIT_EXECUTABLE} apply --whitespace=nowarn --ignore-space-change
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/litehtml-patches/0001-add-document-rebuild-render-tree.patch
)
set(LITEHTML_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(litehtml)

add_subdirectory(third_party/qlitehtml)

qt_standard_project_setup()

set(PROJECT_SOURCES
        src/main.cpp
        
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

        src/core/markdown/MarkdownRenderer.h
        src/core/markdown/MarkdownRenderer.cpp

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
        src/ui/screen/chat/ChatHeader.h
        src/ui/screen/chat/ChatHeader.cpp
        src/ui/screen/chat/ChatAnchorBar.h
        src/ui/screen/chat/ChatAnchorBar.cpp
        src/ui/screen/chat/ChatInputBox.h
        src/ui/screen/chat/ChatInputBox.cpp
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
        src/core/settings/providers/CoreSettingsProvider.h
        src/core/settings/providers/CoreSettingsProvider.cpp
        src/ui/screen/settings/ISettingsUIFactory.h
        src/ui/screen/settings/SettingsUIRegistry.h
        src/ui/screen/settings/SettingsUIRegistry.cpp
        src/ui/screen/settings/factories/CoreSettingsUIFactory.h
        src/ui/screen/settings/factories/CoreSettingsUIFactory.cpp
        src/ui/screen/settings/SettingsPage.h
        src/ui/screen/settings/SettingsPage.cpp

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
        src/data/repository/SqliteProjectRepository.h
        src/data/repository/SqliteModelRepository.h
        src/data/repository/SqliteModelRepository.cpp
        src/data/repository/JsonlMessageRepository.h
        src/data/repository/JsonlMessageRepository.cpp

        src/core/context/ContextManager.h
        src/core/context/ContextManager.cpp
        src/core/context/PromptBuilder.h
        src/core/context/PromptBuilder.cpp
        src/core/model/ModelRegistry.h
        src/core/model/ModelRegistry.cpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

add_subdirectory(res)

set_target_properties(${PROJECT_NAME}
        PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${md4c_SOURCE_DIR}/src
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
        qlitehtml
        md4c
        md4c-html
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

