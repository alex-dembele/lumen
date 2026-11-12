cmake_minimum_required(VERSION 3.24)
project(Lumen VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12
    GIT_SHALLOW TRUE)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW TRUE)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 78770d85c4d94787a56c7859017da963bc4f3439)

FetchContent_Declare(atrac9
    GIT_REPOSITORY https://github.com/shadps4-emu/ext-LibAtrac9.git
    GIT_TAG 946e05a9212976626a9f5e52f29c0a7202871f29)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(SDL3 imgui stb atrac9 json)

file(GLOB ATRAC9_SOURCES ${atrac9_SOURCE_DIR}/C/src/*.c)
add_library(atrac9 STATIC ${ATRAC9_SOURCES})
target_include_directories(atrac9 PUBLIC ${atrac9_SOURCE_DIR}/C/src)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

add_executable(lumen WIN32
    src/main.cpp
    src/app.cpp
    src/sfo.cpp
    src/library.cpp
    src/settings.cpp
    src/emulator.cpp
    src/audio.cpp
    src/at9.cpp
    src/compat.cpp
    src/textures.cpp
    src/ui/theme.cpp
    src/ui/home.cpp
    src/ui/settings_panel.cpp
    src/ui/game_info.cpp)
target_include_directories(lumen PRIVATE src ${stb_SOURCE_DIR})
target_link_libraries(lumen PRIVATE imgui SDL3::SDL3 atrac9 nlohmann_json::nlohmann_json)

if(MSVC)
    target_compile_options(lumen PRIVATE /W4 /utf-8)
else()
    target_compile_options(lumen PRIVATE -Wall -Wextra)
endif()
