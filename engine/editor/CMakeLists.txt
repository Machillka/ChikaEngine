set(TARGET_NAME ChikaEditor)
set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE EDITOR_HEADERS "${CURRENT_DIR}/include/*.h")
file(GLOB_RECURSE EDITOR_SOURCES "${CURRENT_DIR}/src/*.cpp")

add_executable(${TARGET_NAME} main.cpp ${EDITOR_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${CURRENT_DIR}/include> )
target_link_libraries(
    ${TARGET_NAME}
    PRIVATE 
        ChikaCore
        ChikaPlatform
        ChikaRender
        ChikaFramework
        ChikaInput
        ChikaTime
)