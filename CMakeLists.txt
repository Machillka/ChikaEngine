cmake_minimum_required(VERSION 3.10)

project(ChikaEngine VERSION 0.0.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_COMPILE_FEATURES ON)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_compile_definitions(CHIKA_DEBUG)
add_subdirectory(engine)
target_compile_definitions(ChikaEngine PRIVATE
    JPH_USE_SSE4_1
)