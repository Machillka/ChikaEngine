set(TARGET_NAME ChikaRuntime)

add_subdirectory(Core)
add_subdirectory(Profiler)
add_subdirectory(Jobs)
add_subdirectory(Asset)
add_subdirectory(Platform)
add_subdirectory(RHI)
add_subdirectory(Render)
add_subdirectory(Resource)
add_subdirectory(Input)
add_subdirectory(Time)
add_subdirectory(Physics)
add_subdirectory(Framework)
add_subdirectory(Scripts)
add_subdirectory(Project)

add_library(${TARGET_NAME} INTERFACE)

target_link_libraries(${TARGET_NAME} INTERFACE
    ChikaCore
    ChikaProfiler
    ChikaJobs
    ChikaAsset
    ChikaPlatform
    ChikaRHI
    ChikaRender
    ChikaResource
    ChikaInput
    ChikaTime
    ChikaPhysics
    ChikaFramework
    ChikaScripts
    ChikaProject
)
