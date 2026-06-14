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

add_test(NAME Chika.CoreBoundary COMMAND ChikaCoreBoundaryTests)
add_test(NAME Chika.SceneIntegration COMMAND ChikaSceneIntegrationTests)
add_test(NAME Chika.AssetPipeline COMMAND ChikaAssetPipelineTests)
add_test(NAME Chika.RenderBaseline COMMAND ChikaRenderBaselineTests)
add_test(NAME Chika.ShaderInterface COMMAND ChikaShaderInterfaceTests)

set_tests_properties(Chika.CoreBoundary Chika.SceneIntegration Chika.AssetPipeline Chika.RenderBaseline Chika.ShaderInterface PROPERTIES
    WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
)
