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

if(CHIKA_BUILD_BENCHMARKS)
    add_executable(ChikaBenchmarkTests
        unit/BenchmarkTests.cpp
    )
    target_link_libraries(ChikaBenchmarkTests PRIVATE ChikaBenchmarkCore)
endif()

add_test(NAME Chika.CoreBoundary COMMAND ChikaCoreBoundaryTests)
add_test(NAME Chika.SceneIntegration COMMAND ChikaSceneIntegrationTests)
add_test(NAME Chika.AssetPipeline COMMAND ChikaAssetPipelineTests)
add_test(NAME Chika.RenderBaseline COMMAND ChikaRenderBaselineTests)
add_test(NAME Chika.ShaderInterface COMMAND ChikaShaderInterfaceTests)
add_test(NAME Chika.ProjectDescriptor COMMAND ChikaProjectDescriptorTests)
if(CHIKA_BUILD_BENCHMARKS)
    add_test(NAME Chika.Benchmark COMMAND ChikaBenchmarkTests)
endif()
add_test(
    NAME Chika.RuntimeBoundary
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_ROOT_DIR=${PROJECT_ROOT_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RuntimeBoundaryCheck.cmake
)

set_tests_properties(Chika.CoreBoundary Chika.SceneIntegration Chika.AssetPipeline Chika.RenderBaseline Chika.ShaderInterface Chika.ProjectDescriptor Chika.RuntimeBoundary PROPERTIES
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
