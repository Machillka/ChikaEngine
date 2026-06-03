set(THIRD_PARTY_PATH "${ENGINE_ROOT_DIR}/ThirdParty")
set(TARGET_NAME "ChikaThirdParty")

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW docs" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate GLFW install target" FORCE)
add_subdirectory(${THIRD_PARTY_PATH}/glfw)

# Vulkan
find_package(Vulkan REQUIRED)
add_subdirectory(${THIRD_PARTY_PATH}/VulkanMemoryAllocator)

# imgui
find_package(OpenGL REQUIRED)
include(${THIRD_PARTY_PATH}/imgui.cmake)

# fmt
set(FMT_DOC OFF CACHE BOOL "Generate fmt docs" FORCE)
set(FMT_TEST OFF CACHE BOOL "Generate fmt tests" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Generate fmt install target" FORCE)
add_subdirectory(${THIRD_PARTY_PATH}/fmt)

# json
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${THIRD_PARTY_PATH}/json/include)

# obj loader
add_subdirectory(${THIRD_PARTY_PATH}/objloader)

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${THIRD_PARTY_PATH}/stb)

# Jolt
set(TARGET_UNIT_TESTS OFF CACHE BOOL "Build unit tests" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "Build samples" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "Build hello world" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "Build Jolt viewer" FORCE)
set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "Generate debug symbols" FORCE)
add_subdirectory(${THIRD_PARTY_PATH}/jolt/Build JoltPhysics)

# pybind11
set(PYBIND11_INSTALL OFF CACHE BOOL "" FORCE)
set(PYBIND11_PYTHON_VERSION 3)
add_subdirectory(${THIRD_PARTY_PATH}/pybind11)

# tinygltf
add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE ${THIRD_PARTY_PATH}/tinygltf)

# Header-only wrappers with one translation unit for required implementations.
file(GLOB THIRD_PARTY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
add_library(ChikaThirdPartyObjects OBJECT ${THIRD_PARTY_SOURCES})
target_link_libraries(ChikaThirdPartyObjects PRIVATE
    stb
    tinygltf
    GPUOpen::VulkanMemoryAllocator
    Vulkan::Vulkan
)
target_include_directories(ChikaThirdPartyObjects PRIVATE ${THIRD_PARTY_PATH})

# Output
add_library(${TARGET_NAME} INTERFACE)
target_link_libraries(${TARGET_NAME} INTERFACE
    glfw
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    imgui
    fmt::fmt
    nlohmann_json
    tinyobjloader
    stb
    Jolt
    pybind11::module
    pybind11::embed
    tinygltf
    $<TARGET_OBJECTS:ChikaThirdPartyObjects>
)

target_include_directories(${TARGET_NAME} INTERFACE ${THIRD_PARTY_PATH})
