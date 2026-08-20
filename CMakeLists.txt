cmake_minimum_required(VERSION 3.16)
project(ForgeAI LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
        COMPONENTS
        Core
        Gui
        Widgets
)

include(FetchContent)
FetchContent_Declare(
        fluentqt
        GIT_REPOSITORY https://github.com/calvinhxx/Fluent-Qt.git
        GIT_TAG v1.7.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

FetchContent_Declare(
        qwindowkit
        GIT_REPOSITORY https://github.com/stdware/qwindowkit.git
        GIT_TAG 1.5.0
)
FetchContent_MakeAvailable(qwindowkit)

qt_standard_project_setup()

set(PROJECT_SOURCES
        src/main.cpp

                
        src/ui/animation/AnimatedGlobalNavVisualSource.h
        src/ui/animation/AnimatedBackVisualSource.h
        src/ui/animation/AnimatedIcon.cpp
        src/ui/animation/AnimatedIcon.h
        src/ui/animation/AnimatedSettingsVisualSource.h
        src/ui/animation/AnimatedVisualSource.h

        src/ui/base/BaseViewModel.h

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
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME}
        PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        QWindowKit::Widgets
        FluentQt::FluentQt
)

if (WIN32)
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
