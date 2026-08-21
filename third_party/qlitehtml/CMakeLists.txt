cmake_minimum_required(VERSION 3.16)
project(qlitehtml)

add_library(qlitehtml STATIC
    src/container/container_qpainter.cpp
    src/container/container_painting.cpp
    src/container/container_selection.cpp
    src/container/container_serializer.cpp
    src/elements/element_checkbox.cpp
    src/qlitehtmlwidget.cpp
    src/qlitehtmlsearchwidget.cpp
)

# Headers listed for IDE visibility and install/export tooling.
target_sources(qlitehtml PRIVATE
    src/container/container_internal.h
    src/container/container_qpainter.h
    src/container/container_qpainter_p.h
    src/elements/element_checkbox.h
    src/qlitehtml_global.h
    src/qlitehtmlwidget.h
    src/qlitehtmlsearchwidget.h
)

set_target_properties(qlitehtml PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_compile_definitions(qlitehtml
    PUBLIC
        QLITEHTML_STATIC_LIBRARY
)

target_link_libraries(qlitehtml
    PUBLIC
        Qt::Widgets
        litehtml
)

# Public headers live under src/, so consumers include them as
# #include "qlitehtmlwidget.h" without the src/ prefix.
target_include_directories(qlitehtml
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
