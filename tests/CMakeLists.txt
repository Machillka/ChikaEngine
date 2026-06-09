add_executable(ChikaCoreBoundaryTests
    unit/CoreBoundaryTests.cpp
)
target_link_libraries(ChikaCoreBoundaryTests PRIVATE ChikaFramework)

add_executable(ChikaSceneIntegrationTests
    integration/SceneIntegrationTests.cpp
)
target_link_libraries(ChikaSceneIntegrationTests PRIVATE ChikaEngine)

add_test(NAME Chika.CoreBoundary COMMAND ChikaCoreBoundaryTests)
add_test(NAME Chika.SceneIntegration COMMAND ChikaSceneIntegrationTests)

set_tests_properties(Chika.CoreBoundary Chika.SceneIntegration PROPERTIES
    WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
)
