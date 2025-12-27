set(TARGET_NAME EngineCore)

set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 收集所有头文件和源文件
file(GLOB_RECURSE CORE_HEADER_FILES "${CURRENT_DIR}/*.h")
file(GLOB_RECURSE CORE_SOURCE_FILES "${CURRENT_DIR}/*.cpp")

# IDE 分组
source_group(TREE "${CURRENT_DIR}" FILES ${CORE_HEADER_FILES} ${CORE_SOURCE_FILES})

# 创建库
add_library(${TARGET_NAME} STATIC ${CORE_HEADER_FILES} ${CORE_SOURCE_FILES})

target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CURRENT_DIR}       # 让 core/ 成为 include 根目录
)
