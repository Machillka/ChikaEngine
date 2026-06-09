set(TARGET_NAME ChikaRHI)

file(GLOB_RECURSE RHI_PUBLIC_HEADERS CONFIGURE_DEPENDS "include/*.h" "include/*.hpp")
file(GLOB_RECURSE RHI_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(${TARGET_NAME} STATIC
    ${RHI_PUBLIC_HEADERS}
    ${RHI_SOURCES}
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

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Header Files" FILES ${RHI_PUBLIC_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${RHI_SOURCES})
