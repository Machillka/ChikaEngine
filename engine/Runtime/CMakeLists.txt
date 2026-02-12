set(TARGET_NAME ChikaRuntime)

add_subdirectory(Core)
add_subdirectory(Platform)
add_subdirectory(Render)
add_subdirectory(Resource)
add_subdirectory(Input)
add_subdirectory(Time)
add_subdirectory(Physics)
add_subdirectory(Framework)

add_library(${TARGET_NAME} INTERFACE)

target_link_libraries(${TARGET_NAME} INTERFACE 
    ChikaCore 
    ChikaPlatform
    ChikaRender
    ChikaResource
    ChikaInput
    ChikaTime
    ChikaPhysics 
    ChikaFramework
)