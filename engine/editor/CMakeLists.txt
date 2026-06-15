set(TARGET_NAME ChikaEditor)

file(GLOB_RECURSE EDITOR_SRCS CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE EDITOR_HDRS CONFIGURE_DEPENDS "include/*.h" "include/*.hpp")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Core" FILES ${EDITOR_SRCS} ${EDITOR_HDRS})

add_executable(${TARGET_NAME}
    ${EDITOR_SRCS}
    ${EDITOR_HDRS}
)

target_include_directories(${TARGET_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(${TARGET_NAME}
    PRIVATE
        ChikaEngine
        ChikaRuntime
        imgui
)
set_target_properties(${TARGET_NAME} PROPERTIES
    VS_DEBUGGING_WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)
