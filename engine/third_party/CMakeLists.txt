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

# Output
add_library(${TARGET_NAME} INTERFACE)
target_link_libraries(${TARGET_NAME} INTERFACE glfw glad)