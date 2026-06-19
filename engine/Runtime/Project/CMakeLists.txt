set(TARGET_NAME ChikaProject)

file(GLOB_RECURSE PROJECT_PUBLIC_HEADERS CONFIGURE_DEPENDS "include/*.h" "include/*.hpp")
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(${TARGET_NAME} STATIC
    ${PROJECT_PUBLIC_HEADERS}
    ${PROJECT_SOURCES}
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        ChikaAsset
        ChikaPlatform
        ChikaRender
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Header Files" FILES ${PROJECT_PUBLIC_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${PROJECT_SOURCES})
