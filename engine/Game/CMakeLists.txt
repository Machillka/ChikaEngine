set(TARGET_NAME ChikaGame)

file(GLOB_RECURSE GAME_SRCS CONFIGURE_DEPENDS "src/*.cpp")

add_executable(${TARGET_NAME}
    ${GAME_SRCS}
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        ChikaEngine
)

set_target_properties(${TARGET_NAME} PROPERTIES
    VS_DEBUGGING_WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)
