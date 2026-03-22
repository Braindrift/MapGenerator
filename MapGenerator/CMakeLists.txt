# CMakeList.txt : CMake project for MapGenerator, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)
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

add_executable(MapGenerator 
    "source/MapGenerator.cpp" 
    "headers/World.h" 
    "source/World.cpp"
    "source/Game.cpp")

target_link_libraries(MapGenerator 
    PRIVATE 
    SFML::Graphics
)

target_include_directories(MapGenerator PRIVATE headers)