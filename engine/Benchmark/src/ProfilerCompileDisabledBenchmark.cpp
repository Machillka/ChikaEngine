#undef CHIKA_ENABLE_PROFILING
#define CHIKA_ENABLE_PROFILING 0
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

void ProfilerBenchmarkWorkKernel();

/** @brief Measures the exact code-elided macro path in a dedicated translation unit. */
void ProfilerCompileDisabledIteration()
{
    CHIKA_PROFILE_SCOPE("ProfilerBenchmark.Work");
    ProfilerBenchmarkWorkKernel();
}
