# CMakeList.txt : CMake project for MapGenerator, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.5)

project(MapGenerator)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(
    ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(ImGui)

FetchContent_Declare(
    ImGui-SFML
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG v3.0
)
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ImGui-SFML)

add_executable(MapGenerator
    "source/MapGenerator.cpp"
    "headers/World.h"
    "source/World.cpp"
    "source/Game.cpp"
    "headers/InfoPanel.h"
    "source/InfoPanel.cpp"
    "headers/Camera.h"
    "source/Camera.cpp")

target_link_libraries(MapGenerator
    PRIVATE
    SFML::Graphics
    ImGui-SFML::ImGui-SFML
)

target_include_directories(MapGenerator PRIVATE headers)
