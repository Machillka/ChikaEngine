set(TARGET_NAME "ChikaRenderOpenGL")
file(GLOB OPENGL_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB OPENGL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${TARGET_NAME} STATIC
    ${OPENGL_HEADERS}
    ${OPENGL_SOURCES}
)

target_include_directories(
    ${TARGET_NAME}
    PUBLIC
        ${CMAKE_SOURCE_DIR}/engine
)

# link 3rd party
target_link_libraries(${TARGET_NAME}
    PUBLIC
        ChikaThirdParty
)
