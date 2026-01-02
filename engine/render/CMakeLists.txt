set(TARGET_NAME ChikaRender)
set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE RENDER_HEADERS "${CURRENT_DIR}/*.h")
file(GLOB_RECURSE RENDER_SOURCES "${CURRENT_DIR}/*.cpp")

add_library(${TARGET_NAME} STATIC ${RENDER_HEADERS} ${RENDER_SOURCES})

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/engine)

target_link_libraries(${TARGET_NAME} PUBLIC ChikaThirdParty ChikaCore ChikaPlatform)
add_subdirectory(rhi/opengl)
target_link_libraries(${TARGET_NAME} PUBLIC ChikaRenderOpenGL)