set(THIRD_PARTY_PATH "${ENGINE_ROOT_DIR}/third_party")
set(TARGET_NAME "ChikaThirdParty")
# Add GLFW
add_subdirectory(${THIRD_PARTY_PATH}/glfw)

# Add GLAD
add_library(glad STATIC ${THIRD_PARTY_PATH}/glad/src/glad.c)
target_include_directories(glad PUBLIC ${THIRD_PARTY_PATH}/glad/include)

# Add imgui
find_package(OpenGL REQUIRED)
include(${THIRD_PARTY_PATH}/imgui.cmake)

# fmt
add_subdirectory(${THIRD_PARTY_PATH}/fmt)

#json
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${THIRD_PARTY_PATH}/json/include)

#obj loader
add_subdirectory(${THIRD_PARTY_PATH}/objloader)

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${THIRD_PARTY_PATH}/stb)


# Output
add_library(${TARGET_NAME} INTERFACE)
target_link_libraries(${TARGET_NAME} INTERFACE
    glfw
    glad
    imgui
    fmt::fmt
    nlohmann_json
    tinyobjloader
    stb
)

target_include_directories(${TARGET_NAME} INTERFACE ${THIRD_PARTY_PATH})