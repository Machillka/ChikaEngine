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

add_executable(ChikaProfilerOverheadBenchmark
    src/ProfilerOverheadBenchmark.cpp
    src/ProfilerCompileDisabledBenchmark.cpp
)
target_link_libraries(ChikaProfilerOverheadBenchmark PRIVATE ChikaProfiler nlohmann_json)
target_compile_definitions(ChikaProfilerOverheadBenchmark PRIVATE
    CHIKA_PROFILER_BENCHMARK_BUILD_TYPE="$<CONFIG>"
)
set_target_properties(ChikaProfilerOverheadBenchmark PROPERTIES
    VS_DEBUGGING_WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

add_executable(ChikaJobBenchmark src/JobBenchmark.cpp)
target_link_libraries(ChikaJobBenchmark PRIVATE ChikaJobs nlohmann_json)
target_compile_definitions(ChikaJobBenchmark PRIVATE CHIKA_JOB_BENCHMARK_BUILD_TYPE="$<CONFIG>")
set_target_properties(ChikaJobBenchmark PROPERTIES
    VS_DEBUGGING_WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)
