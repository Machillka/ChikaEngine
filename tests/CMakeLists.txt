add_executable(ChikaCoreBoundaryTests
    unit/CoreBoundaryTests.cpp
)
target_link_libraries(ChikaCoreBoundaryTests PRIVATE ChikaFramework)

add_executable(ChikaSceneIntegrationTests
    integration/SceneIntegrationTests.cpp
)
target_link_libraries(ChikaSceneIntegrationTests PRIVATE ChikaEngine)

add_executable(ChikaAssetPipelineTests
    integration/AssetPipelineTests.cpp
)
target_link_libraries(ChikaAssetPipelineTests PRIVATE ChikaAsset)

add_executable(ChikaAssetJobIntegrationTests
    integration/AssetJobIntegrationTests.cpp
)
target_link_libraries(ChikaAssetJobIntegrationTests PRIVATE ChikaAsset ChikaJobs ChikaProfiler)

add_executable(ChikaRenderBaselineTests
    unit/RenderBaselineTests.cpp
)
target_link_libraries(ChikaRenderBaselineTests PRIVATE ChikaRender)

add_executable(ChikaShaderInterfaceTests
    unit/ShaderInterfaceTests.cpp
)
target_link_libraries(ChikaShaderInterfaceTests PRIVATE ChikaAsset)

add_executable(ChikaProjectDescriptorTests
    unit/ProjectDescriptorTests.cpp
)
target_link_libraries(ChikaProjectDescriptorTests PRIVATE ChikaProject)

add_executable(ChikaProfilerTests
    unit/ProfilerTests.cpp
    unit/ProfilerDisabledMacroTests.cpp
)
target_link_libraries(ChikaProfilerTests PRIVATE ChikaProfiler nlohmann_json)

add_executable(ChikaProfilerTimelineModelTests
    unit/ProfilerTimelineModelTests.cpp
    ${PROJECT_ROOT_DIR}/engine/Editor/src/ProfilerTimelineModel.cpp
)
target_include_directories(ChikaProfilerTimelineModelTests PRIVATE ${PROJECT_ROOT_DIR}/engine/Editor/include)
target_link_libraries(ChikaProfilerTimelineModelTests PRIVATE ChikaProfiler)

add_executable(ChikaJobSystemTests
    unit/JobSystemTests.cpp
)
target_link_libraries(ChikaJobSystemTests PRIVATE ChikaJobs)

add_executable(ChikaJobStressTests
    stress/JobStealingStressTests.cpp
)
target_link_libraries(ChikaJobStressTests PRIVATE ChikaJobs)

if(CHIKA_BUILD_BENCHMARKS)
    add_executable(ChikaBenchmarkTests
        unit/BenchmarkTests.cpp
    )
    target_link_libraries(ChikaBenchmarkTests PRIVATE ChikaBenchmarkCore)
endif()

add_test(NAME Chika.CoreBoundary COMMAND ChikaCoreBoundaryTests)
add_test(NAME Chika.SceneIntegration COMMAND ChikaSceneIntegrationTests)
add_test(NAME Chika.AssetPipeline COMMAND ChikaAssetPipelineTests)
add_test(NAME Chika.AssetJobs COMMAND ChikaAssetJobIntegrationTests)
add_test(NAME Chika.RenderBaseline COMMAND ChikaRenderBaselineTests)
add_test(NAME Chika.ShaderInterface COMMAND ChikaShaderInterfaceTests)
add_test(NAME Chika.ProjectDescriptor COMMAND ChikaProjectDescriptorTests)
add_test(NAME Chika.Profiler COMMAND ChikaProfilerTests)
add_test(NAME Chika.ProfilerTimelineModel COMMAND ChikaProfilerTimelineModelTests)
add_test(NAME Chika.JobSystem COMMAND ChikaJobSystemTests)
add_test(NAME Chika.JobStress COMMAND ChikaJobStressTests)
set_tests_properties(Chika.JobStress PROPERTIES LABELS "stress" TIMEOUT 120)
if(CHIKA_BUILD_BENCHMARKS)
    add_test(NAME Chika.Benchmark COMMAND ChikaBenchmarkTests)
endif()
add_test(
    NAME Chika.RuntimeBoundary
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_ROOT_DIR=${PROJECT_ROOT_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RuntimeBoundaryCheck.cmake
)

set_tests_properties(Chika.CoreBoundary Chika.SceneIntegration Chika.AssetPipeline Chika.AssetJobs Chika.RenderBaseline Chika.ShaderInterface Chika.ProjectDescriptor Chika.Profiler Chika.ProfilerTimelineModel Chika.JobSystem Chika.RuntimeBoundary PROPERTIES
    WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
)
if(CHIKA_BUILD_BENCHMARKS)
    set_tests_properties(Chika.Benchmark PROPERTIES WORKING_DIRECTORY "${PROJECT_ROOT_DIR}")
endif()

if(CHIKA_BUILD_GAME AND CHIKA_ENABLE_RUNTIME_SMOKE_TESTS)
    add_test(NAME Chika.GameSmoke COMMAND ChikaGame --smoke-frames 3)
    set_tests_properties(Chika.GameSmoke PROPERTIES
        WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
        LABELS "smoke"
        TIMEOUT 30
    )
endif()
