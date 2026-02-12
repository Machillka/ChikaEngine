#pragma once

// For different platforms
#if defined(_WIN32)
#define CHIKA_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#include <signal.h>
#define CHIKA_DEBUG_BREAK() raise(SIGTRAP)
#else
#include <cstdlib>
#define CHIKA_DEBUG_BREAK() std::adort()
#endif