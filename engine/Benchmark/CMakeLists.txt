set(BENCHMARK_CORE_SOURCES
    src/BenchmarkOptions.cpp
    src/BenchmarkResult.cpp
    src/BenchmarkRunner.cpp
    src/BenchmarkSceneFactory.cpp
    src/BenchmarkSceneLayout.cpp
    src/SampleStatistics.cpp
)

add_library(ChikaBenchmarkCore STATIC ${BENCHMARK_CORE_SOURCES})
target_include_directories(ChikaBenchmarkCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ChikaBenchmarkCore PUBLIC ChikaEngine)
target_compile_definitions(ChikaBenchmarkCore PRIVATE
    CHIKA_BENCHMARK_ENGINE_VERSION="${PROJECT_VERSION}"
    CHIKA_BENCHMARK_BUILD_TYPE="$<CONFIG>"
)

add_executable(ChikaBenchmark src/main.cpp)
target_link_libraries(ChikaBenchmark PRIVATE ChikaBenchmarkCore)
set_target_properties(ChikaBenchmark PROPERTIES
    VS_DEBUGGING_WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)
