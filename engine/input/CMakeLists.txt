set(TARGET_NAME ChikaInput)

set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB INPUT_HEADER_FILES "${CURRENT_DIR}/*.h")
file(GLOB INPUT_SOURCE_FILES "${CURRENT_DIR}/*.cpp")

source_group(TREE "${CURRENT_DIR}" FILES ${INPUT_HEADER_FILES} ${INPUT_SOURCE_FILES})

add_library(${TARGET_NAME} STATIC ${INPUT_HEADER_FILES} ${INPUT_SOURCE_FILES})

target_include_directories(${TARGET_NAME}
    PUBLIC
    ${CURRENT_DIR}
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        ChikaThirdParty
        ChikaCore
)