cmake_minimum_required(VERSION 3.24)

project(ChikaEngine VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WARN_UNINITIALIZED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

add_compile_definitions(CHIKA_DEBUG)

message(STATUS "[ChikaEngine] Bootstrapping isolated Python environment via uv...")
# 查找本地 uv 环境
find_program(UV_EXECUTABLE uv REQUIRED)
execute_process(
    COMMAND ${UV_EXECUTABLE} sync
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE UV_SYNC_RESULT
)
if(NOT UV_SYNC_RESULT EQUAL 0)
    message(FATAL_ERROR "uv sync failed! Check your internet connection or pyproject.toml.")
endif()
if(CMAKE_HOST_WIN32)
    set(ISOLATED_PYTHON_EXE "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
else()
    set(ISOLATED_PYTHON_EXE "${CMAKE_SOURCE_DIR}/.venv/bin/python")
endif()

if(NOT EXISTS "${ISOLATED_PYTHON_EXE}")
    message(FATAL_ERROR "Isolated Python executable not found at ${ISOLATED_PYTHON_EXE}. Did uv sync fail?")
endif()
set(Python3_EXECUTABLE "${ISOLATED_PYTHON_EXE}" CACHE FILEPATH "Path to strict isolated Python" FORCE)
set(Python3_FIND_VIRTUALENV FIRST)
set(Python3_FIND_STRATEGY LOCATION)
find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)

message(STATUS "[ChikaEngine] Found Python Include: ${Python3_INCLUDE_DIRS}")
message(STATUS "[ChikaEngine] Found Python Libraries: ${Python3_LIBRARIES}")

if(CMAKE_HOST_WIN32)
    message(STATUS "[ChikaEngine] Resolving Python Runtime DLL for Windows deployment...")
    set(PYTHON_GET_DLL_SCRIPT
        "import sys, os; \
        dll_name = f'python{sys.version_info.major}{sys.version_info.minor}.dll'; \
        path = os.path.join(sys.base_prefix, dll_name); \
        print(path if os.path.exists(path) else '', end='')"
    )
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "${PYTHON_GET_DLL_SCRIPT}"
        OUTPUT_VARIABLE PYTHON_RUNTIME_DLL
        RESULT_VARIABLE PYTHON_DLL_FIND_RESULT
    )
    if(PYTHON_DLL_FIND_RESULT EQUAL 0 AND EXISTS "${PYTHON_RUNTIME_DLL}")
        message(STATUS "[ChikaEngine] Found Python DLL: ${PYTHON_RUNTIME_DLL}")
        # 确保输出目录存在
        file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        file(COPY "${PYTHON_RUNTIME_DLL}" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        message(STATUS "[ChikaEngine] Successfully copied Python DLL to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    else()
        message(WARNING "[ChikaEngine] Could not automatically locate the Python DLL. EXE might fail to run.")
    endif()
endif()


# 加入 engine。构建
add_subdirectory(engine)

target_compile_definitions(ChikaEngine PRIVATE
    JPH_USE_SSE4_1
)