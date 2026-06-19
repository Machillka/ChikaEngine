#undef CHIKA_ENABLE_PROFILING
#define CHIKA_ENABLE_PROFILING 0
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

/** @brief Verifies compile-disabled macros do not evaluate instrumentation arguments. */
bool ProfilerCompileDisabledDoesNotEvaluate()
{
    int sideEffect = 0;
    CHIKA_PROFILE_COUNTER("Disabled.Counter", ++sideEffect);
    CHIKA_PROFILE_SCOPE("Disabled.Scope");
    return sideEffect == 0;
}
