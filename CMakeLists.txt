cmake_minimum_required(VERSION 3.16)
project(FreeSpaceFinder VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)

qt_add_executable(FreeSpaceFinder
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/filenode.h
    src/scanner.h
    src/scanner.cpp
    src/filemodel.h
    src/filemodel.cpp
    src/analyzer.h
    src/analyzer.cpp
    src/aiclient.h
    src/aiclient.cpp
    src/summarypanel.h
    src/summarypanel.cpp
    src/aichatwidget.h
    src/aichatwidget.cpp
        main.cpp
        main.cpp
        main.cpp
)

target_link_libraries(FreeSpaceFinder PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
)

target_include_directories(FreeSpaceFinder PRIVATE src)

target_compile_options(FreeSpaceFinder PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CONFIG:Debug>:-g -O0 -fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-O2>
)

set_target_properties(FreeSpaceFinder PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Symlink compile_commands.json to project root for clangd
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    )
endif()
