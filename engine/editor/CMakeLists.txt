set(TARGET_NAME ChikaEditor)

file(GLOB_RECURSE EDITOR_SRCS "src/*.cpp")
file(GLOB_RECURSE EDITOR_HDRS "include/*.h")

set(ADAPTER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/adapter/imgui")
file(GLOB_RECURSE ADAPTER_SRCS "${ADAPTER_DIR}/src/*.cpp")
file(GLOB_RECURSE ADAPTER_HDRS "${ADAPTER_DIR}/include/*.h")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Core" FILES ${EDITOR_SRCS} ${EDITOR_HDRS})
source_group(TREE "${ADAPTER_DIR}" PREFIX "Adapter/ImGui" FILES ${ADAPTER_SRCS} ${ADAPTER_HDRS})


add_executable(${TARGET_NAME}
    ${EDITOR_SRCS}
    ${EDITOR_HDRS}
    ${ADAPTER_SRCS}
    ${ADAPTER_HDRS}
)
# NOTE: 仅带入 imgui 的 适配器 如果写了其他的记得修改
target_include_directories(${TARGET_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/adapter/imgui"
)
target_link_libraries(${TARGET_NAME}
    PRIVATE
        ChikaEngine
        ChikaRuntime
)
set_target_properties(${TARGET_NAME} PROPERTIES
    VS_DEBUGGING_WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
    # WIN32_EXECUTABLE ON 
)