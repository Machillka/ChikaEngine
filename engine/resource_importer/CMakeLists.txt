set(TARGET_NAME ChikaResource)

set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE RESOURCE_HEADER_FILES "${CURRENT_DIR}/include/*.h")
file(GLOB_RECURSE RESOURCE_SOURCE_FILES "${CURRENT_DIR}/src/*.cpp")

source_group(TREE "${CURRENT_DIR}" FILES ${RESOURCE_HEADER_FILES} ${RESOURCE_SOURCE_FILES})

add_library(${TARGET_NAME} STATIC ${RESOURCE_HEADER_FILES} ${RESOURCE_SOURCE_FILES})

target_include_directories(${TARGET_NAME}
    PUBLIC
    ${CURRENT_DIR}
)
target_link_libraries(${TARGET_NAME}
    PUBLIC
        ChikaThirdParty
        ChikaCore
        ChikaRender
)
