set(TARGET_NAME ChikaPhysics)

set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE PHYSICS_HEADER_FILES "${CURRENT_DIR}/include/*.h")
file(GLOB_RECURSE PHYSICS_SOURCE_FILES "${CURRENT_DIR}/src/*.cpp")

source_group(TREE "${CURRENT_DIR}" FILES ${PHYSICS_HEADER_FILES} ${PHYSICS_SOURCE_FILES})

add_library(${TARGET_NAME} STATIC ${PHYSICS_HEADER_FILES} ${PHYSICS_SOURCE_FILES})

target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CURRENT_DIR}
)
target_link_libraries(${TARGET_NAME}
    PUBLIC
        ChikaThirdParty
        ChikaCore
        ChikaFramework
)
