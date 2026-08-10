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

# ImGui is an Editor-only dependency. Standalone Game builds do not configure
# or inherit it through the shared runtime third-party interface.
if(CHIKA_BUILD_EDITOR)
    include(${THIRD_PARTY_PATH}/imgui.cmake)
endif()

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

# TinyEXR v3
# The upstream CMakeLists is only intended for compile tests and changes global
# compiler settings, so ChikaEngine owns the minimal library target here.
file(GLOB TINYEXR_V3_SOURCES CONFIGURE_DEPENDS
    "${THIRD_PARTY_PATH}/tinyexr/src/*.c"
)
list(FILTER TINYEXR_V3_SOURCES EXCLUDE REGEX
    "/(exr_freestanding|exr_spectral|exr_gpu_cuda|exr_vk_vulkan)\\.c$"
)
add_library(tinyexr STATIC
    ${TINYEXR_V3_SOURCES}
    "${THIRD_PARTY_PATH}/tinyexr/deps/zstd/tinyexr_zstd.c"
)
add_library(tinyexr::tinyexr ALIAS tinyexr)
target_include_directories(tinyexr
    PUBLIC
        "${THIRD_PARTY_PATH}/tinyexr/include"
    PRIVATE
        "${THIRD_PARTY_PATH}/tinyexr/src"
        "${THIRD_PARTY_PATH}/tinyexr/deps/zstd"
)
set_target_properties(tinyexr PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    FOLDER "ThirdParty"
)
if(MSVC)
    # MSVC's C11 mode does not support the qualifier form `_Atomic T` used by
    # TinyEXR v3. The affected values are runtime backend function pointers;
    # ChikaEngine keeps the upstream set-before-decode contract, so compiling
    # that qualifier out is safe and avoids patching vendored sources.
    target_compile_definitions(tinyexr PRIVATE
        _CRT_SECURE_NO_WARNINGS
        "_Atomic="
    )
    set_source_files_properties(
        "${THIRD_PARTY_PATH}/tinyexr/src/exr_jph_simd.c"
        PROPERTIES COMPILE_OPTIONS "/FI${ENGINE_ROOT_DIR}/cmake/TinyExrMsvcCompat.h"
    )
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(tinyexr PRIVATE -Wno-macro-redefined)
endif()

# Jolt
set(TARGET_UNIT_TESTS OFF CACHE BOOL "Build unit tests" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "Build samples" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "Build hello world" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "Build Jolt viewer" FORCE)
set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "Generate debug symbols" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "Disable unused Jolt DirectX 12 compute backend" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "Disable unused Jolt Vulkan compute backend" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "Disable unused Jolt Metal compute backend" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "Disable unused Jolt CPU compute backend" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use the dynamic MSVC runtime used by ChikaEngine" FORCE)
add_subdirectory(${THIRD_PARTY_PATH}/jolt/Build JoltPhysics)

# pybind11
set(PYBIND11_INSTALL OFF CACHE BOOL "" FORCE)
set(PYBIND11_PYTHON_VERSION 3)
add_subdirectory(${THIRD_PARTY_PATH}/pybind11)

# tinygltf
add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE ${THIRD_PARTY_PATH}/tinygltf)

# SPIRV-Reflect
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Build spirv-reflect executable" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build a SPIRV-Reflect static library" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "Build the SPIRV-Reflect test suite" FORCE)
set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "Install SPIRV-Reflect targets" FORCE)
add_subdirectory(${THIRD_PARTY_PATH}/SPIRVReflect)

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
    fmt::fmt
    nlohmann_json
    tinyobjloader
    stb
    tinyexr::tinyexr
    Jolt
    pybind11::module
    pybind11::embed
    tinygltf
    spirv-reflect-static
    $<TARGET_OBJECTS:ChikaThirdPartyObjects>
)

target_include_directories(${TARGET_NAME} INTERFACE ${THIRD_PARTY_PATH})
