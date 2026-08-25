cmake_minimum_required(VERSION 3.21)
project(ForgeAI VERSION 1.7.1 LANGUAGES C CXX)

include(qt.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
        COMPONENTS
        Core
        Gui
        Widgets
        Network
        Sql
        Test
)

include(FetchContent)
set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(fluentqt
    GIT_REPOSITORY https://github.com/L-1124/Fluent-Qt.git
    GIT_TAG ff2ee27081458ad1ff9fe83a71c651acef056824
)
FetchContent_Declare(qwindowkit
    GIT_REPOSITORY https://github.com/stdware/qwindowkit.git
    GIT_TAG 35e88f3655720ed0537c7dd1dde243bf4a70c94c
)
FetchContent_Declare(cmark-gfm
    GIT_REPOSITORY https://github.com/github/cmark-gfm.git
    GIT_TAG 587a12bb54d95ac37241377e6ddc93ea0e45439b
)
# Declare every dependency before making any available, so this top-level
# project owns their versions. FETCHCONTENT_SOURCE_DIR_<NAME> remains the
# supported developer/CI escape hatch for an offline local checkout.
FetchContent_MakeAvailable(fluentqt qwindowkit cmark-gfm)

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

find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${QT_DIR}/bin" "${Qt6_DIR}/../../../bin" "${CMAKE_PREFIX_PATH}/bin")

macro(deploy_qt_application target)
    if (WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}>
                $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
            COMMENT "Deploying Qt runtime DLLs for ${target}..."
        )
        if (WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --verbose 0
                    --no-compiler-runtime
                    --no-opengl-sw
                    "$<TARGET_FILE:${target}>"
                COMMENT "Running windeployqt to copy dependencies for ${target}..."
            )
        endif()
    endif()
endmacro()

qt_standard_project_setup()

add_library(ForgeAICore)

# Each first-level source module owns its explicit target source list.
# Keep this explicit rather than using file(GLOB), so adding/removing a
# source remains visible to CMake and to code review.
foreach(module IN ITEMS core network llm ui domain data services application agent app)
    add_subdirectory("src/${module}")
endforeach()
target_compile_features(ForgeAICore PUBLIC cxx_std_23)
target_compile_definitions(ForgeAICore PUBLIC CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
target_include_directories(ForgeAICore PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/extensions
        ${cmark-gfm_BINARY_DIR}/src
        ${cmark-gfm_BINARY_DIR}/extensions
)
target_link_libraries(ForgeAICore PUBLIC
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
target_precompile_headers(ForgeAICore PRIVATE 
    <QObject> <QWidget> <QString> <QNetworkReply> <memory> <vector> <QUuid> <QDateTime> <QList>
)

qt_add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ForgeAICore)

add_subdirectory(res)

set_target_properties(${PROJECT_NAME}
        PROPERTIES
        WIN32_EXECUTABLE TRUE
)

include(CTest)
if (BUILD_TESTING)
    qt_add_executable(MarkdownCoreTests tests/MarkdownCoreTests.cpp)
    target_link_libraries(MarkdownCoreTests PRIVATE ForgeAICore Qt::Test)
    deploy_qt_application(MarkdownCoreTests)
    find_file(QT_QOFFSCREEN_DLL NAMES qoffscreend.dll qoffscreen.dll HINTS "${QT_DIR}/plugins/platforms" "${Qt6_DIR}/../../../plugins/platforms" "${CMAKE_PREFIX_PATH}/plugins/platforms")
    if (WIN32 AND QT_QOFFSCREEN_DLL)
        add_custom_command(TARGET MarkdownCoreTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MarkdownCoreTests>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_QOFFSCREEN_DLL}"
                "$<TARGET_FILE_DIR:MarkdownCoreTests>/platforms/qoffscreend.dll"
            COMMENT "Deploying Qt offscreen platform plugin for MarkdownCoreTests..."
        )
    endif()
    add_test(NAME MarkdownCoreTests COMMAND MarkdownCoreTests)
    set_tests_properties(MarkdownCoreTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")

    qt_add_executable(AgentToolTests tests/AgentToolTests.cpp)
    target_link_libraries(AgentToolTests PRIVATE ForgeAICore Qt::Test Qt::Sql)
    deploy_qt_application(AgentToolTests)
    find_file(QT_QSQLITE_DLL NAMES qsqlited.dll qsqlite.dll HINTS "${QT_DIR}/plugins/sqldrivers" "${Qt6_DIR}/../../../plugins/sqldrivers" "${CMAKE_PREFIX_PATH}/plugins/sqldrivers")
    if (WIN32 AND QT_QSQLITE_DLL)
        add_custom_command(TARGET AgentToolTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:AgentToolTests>/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_QSQLITE_DLL}"
                "$<TARGET_FILE_DIR:AgentToolTests>/sqldrivers/qsqlite.dll"
            COMMENT "Deploying Qt sqlite plugin for AgentToolTests..."
        )
    endif()
    add_test(NAME AgentToolTests COMMAND AgentToolTests)

    qt_add_executable(AgentProtocolTests tests/AgentProtocolTests.cpp)
    target_link_libraries(AgentProtocolTests PRIVATE ForgeAICore Qt::Test)
    deploy_qt_application(AgentProtocolTests)
    add_test(NAME AgentProtocolTests COMMAND AgentProtocolTests)

    qt_add_executable(WorkIntegrationTests tests/WorkIntegrationTests.cpp)
    target_link_libraries(WorkIntegrationTests PRIVATE ForgeAICore Qt::Test)
    deploy_qt_application(WorkIntegrationTests)
    if (WIN32 AND QT_QSQLITE_DLL)
        add_custom_command(TARGET WorkIntegrationTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:WorkIntegrationTests>/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_QSQLITE_DLL}"
                "$<TARGET_FILE_DIR:WorkIntegrationTests>/sqldrivers/qsqlite.dll"
            COMMENT "Deploying Qt sqlite plugin for WorkIntegrationTests..."
        )
    endif()
    add_test(NAME WorkIntegrationTests COMMAND WorkIntegrationTests)
    set_tests_properties(AgentToolTests PROPERTIES TIMEOUT 60)
    set_tests_properties(AgentProtocolTests PROPERTIES TIMEOUT 60)
    set_tests_properties(WorkIntegrationTests PROPERTIES TIMEOUT 60)
endif ()

deploy_qt_application(${PROJECT_NAME})

