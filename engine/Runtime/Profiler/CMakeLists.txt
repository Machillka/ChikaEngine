set(TARGET_NAME ChikaProfiler)

file(GLOB_RECURSE PROFILER_PUBLIC_HEADERS CONFIGURE_DEPENDS "include/*.h" "include/*.hpp")
file(GLOB_RECURSE PROFILER_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(${TARGET_NAME} STATIC
    ${PROFILER_PUBLIC_HEADERS}
    ${PROFILER_SOURCES}
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PRIVATE nlohmann_json)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Header Files" FILES ${PROFILER_PUBLIC_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${PROFILER_SOURCES})
