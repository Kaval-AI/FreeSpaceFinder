cmake_minimum_required(VERSION 3.16)
project(FreeSpaceFinder VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent Test)

# Core (non-GUI) logic — shared by the app and the unit tests
qt_add_library(FreeSpaceFinderCore STATIC
    src/filenode.h
    src/scanner.h
    src/scanner.cpp
    src/filemodel.h
    src/filemodel.cpp
    src/analyzer.h
    src/analyzer.cpp
    src/aiclient.h
    src/aiclient.cpp
)

target_link_libraries(FreeSpaceFinderCore PUBLIC
    Qt6::Core
    Qt6::Network
)

target_include_directories(FreeSpaceFinderCore PUBLIC src)

qt_add_executable(FreeSpaceFinder
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/summarypanel.h
    src/summarypanel.cpp
    src/aichatwidget.h
    src/aichatwidget.cpp
)

qt_add_resources(FreeSpaceFinder "icons"
    PREFIX "/icons"
    BASE resources/icons
    FILES
        resources/icons/app.svg
        resources/icons/add-folder.svg
        resources/icons/remove-folder.svg
        resources/icons/scan.svg
        resources/icons/cancel.svg
        resources/icons/clear.svg
        resources/icons/settings.svg
)

target_link_libraries(FreeSpaceFinder PRIVATE
    FreeSpaceFinderCore
    Qt6::Widgets
    Qt6::Concurrent
)

foreach(target FreeSpaceFinderCore FreeSpaceFinder)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CONFIG:Debug>:-g -O0 -fno-omit-frame-pointer>
        $<$<CONFIG:Release>:-O2>
    )
endforeach()

set_target_properties(FreeSpaceFinder PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

enable_testing()
add_subdirectory(tests)

# Symlink compile_commands.json to project root for clangd
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    )
endif()
