set(TARGET_NAME ChikaTime)

set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB TIME_HEADER_FILES "${CURRENT_DIR}/*.h")
file(GLOB TIME_SOURCE_FILES "${CURRENT_DIR}/*.cpp")

source_group(TREE "${CURRENT_DIR}" FILES ${TIME_HEADER_FILES} ${TIME_SOURCE_FILES})

add_library(${TARGET_NAME} STATIC ${TIME_HEADER_FILES} ${TIME_SOURCE_FILES})

target_include_directories(${TARGET_NAME}
    PUBLIC
    ${CURRENT_DIR}
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        ChikaThirdParty
        ChikaCore
)