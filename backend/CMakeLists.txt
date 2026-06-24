cmake_minimum_required(VERSION 3.12)
project(TopoOptV2_CPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(topoopt
    src/main.cpp
    src/models.cpp
    src/initializer.cpp
    src/routing.cpp
    src/evaluators.cpp
    src/ga.cpp
    src/config_io.cpp
)

target_include_directories(topoopt PRIVATE include)

if (MSVC)
    target_compile_options(topoopt PRIVATE /W4 /utf-8)
else()
    target_compile_options(topoopt PRIVATE -Wall -Wextra -pedantic)
endif()
