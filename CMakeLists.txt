cmake_minimum_required(VERSION 3.21)

project(ClaudeUsageMonitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(ClaudeUsageMonitor WIN32
    src/main.cpp
    src/AccessoryWindow.cpp
    src/AccessoryWindow.h
    src/CredentialProvider.cpp
    src/CredentialProvider.h
    src/UsageClient.cpp
    src/UsageClient.h
    src/UsageModel.h
)

target_link_libraries(ClaudeUsageMonitor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(WIN32)
    target_link_libraries(ClaudeUsageMonitor PRIVATE winhttp)
elseif(UNIX AND NOT APPLE)
    find_package(CURL REQUIRED)
    target_link_libraries(ClaudeUsageMonitor PRIVATE CURL::libcurl)
endif()

if(WIN32)
    target_compile_definitions(ClaudeUsageMonitor PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
