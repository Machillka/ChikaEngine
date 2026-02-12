set(TARGET_NAME ChikaPhysics)

file(GLOB_RECURSE CORE_PUBLIC_HEADERS "include/*.h")
file(GLOB_RECURSE CORE_SOURCES "src/*.cpp")

add_library(${TARGET_NAME} STATIC
    ${CORE_PUBLIC_HEADERS}
    ${CORE_SOURCES}
)

target_include_directories(${TARGET_NAME}
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>    
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        ChikaThirdParty
        ChikaCore
)

reflection_generator(${TARGET_NAME})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Header Files" FILES ${CORE_PUBLIC_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${CORE_SOURCES})