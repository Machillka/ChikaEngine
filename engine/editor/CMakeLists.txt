set(TARGET_NAME ChikaEditor)
set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE EDITOR_HEADERS "${CURRENT_DIR}/include/*.h")
file(GLOB_RECURSE EDITOR_SOURCES "${CURRENT_DIR}/src/*.cpp")

file(GLOB_RECURSE ADAPTER_HEADERS "${CURRENT_DIR}/adapter/*.h")
file(GLOB_RECURSE ADAPTER_SOURCES "${CURRENT_DIR}/adapter/*.cpp")

add_executable(${TARGET_NAME} ${EDITOR_HEADERS} ${EDITOR_SOURCES} ${ADAPTER_HEADERS} ${ADAPTER_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${CURRENT_DIR}/include> )
target_link_libraries(
    ${TARGET_NAME}
    PRIVATE
        ChikaThirdParty
        ChikaCore
        ChikaPlatform
        ChikaRender
        ChikaFramework
        ChikaInput
        ChikaTime
)