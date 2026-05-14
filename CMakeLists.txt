cmake_minimum_required(VERSION 3.14)
project(haiclaude-tui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# FTXUI - use URL for faster download
FetchContent_Declare(ftxui
    URL https://github.com/ArthurSonzogni/ftxui/archive/refs/tags/v6.1.9.tar.gz
)
FetchContent_MakeAvailable(ftxui)

# nlohmann/json - single-header, use URL for faster download
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_executable(haiclaude-tui
    src/main.cpp
    src/config.cpp
    src/launcher.cpp
    src/tui_app.cpp
)

target_link_libraries(haiclaude-tui PRIVATE ftxui::screen ftxui::dom ftxui::component nlohmann_json::nlohmann_json)
target_include_directories(haiclaude-tui PRIVATE src)

install(TARGETS haiclaude-tui DESTINATION bin)
