// Verifies the MSVC-only `__builtin_clz` compatibility shim used to build the
// vendored TinyEXR JPH AVX2 path (engine/ThirdParty/CMakeLists.txt applies
// this header via a forced /FI include on MSVC only).
//
// The header depends on <intrin.h> / _BitScanReverse and is only ever
// injected into MSVC translation units. On non-MSVC toolchains this test
// intentionally has nothing to exercise and reports success, so the same
// test target still builds and passes on the Linux/GCC and macOS/AppleClang
// CI legs.

#include <cstdint>
#include <iostream>

#if defined(_MSC_VER)
#include "TinyExrMsvcCompat.h"
#endif

namespace {
    int g_failures = 0;

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }
}

int main() {
#if defined(_MSC_VER)
    // Direct shim function checks (bypassing the macro rewrite).
    Check(ChikaTinyExrCountLeadingZeros32(0u) == 32u,
          "clz(0) is undefined for _BitScanReverse; the shim must return the defined sentinel 32");
    Check(ChikaTinyExrCountLeadingZeros32(1u) == 31u,
          "clz(1) must report 31 leading zeros in a 32-bit value");
    Check(ChikaTinyExrCountLeadingZeros32(2u) == 30u,
          "clz(2) must report 30 leading zeros (highest set bit at index 1)");
    Check(ChikaTinyExrCountLeadingZeros32(3u) == 30u,
          "clz(3) must count from the highest set bit, ignoring lower set bits");
    Check(ChikaTinyExrCountLeadingZeros32(0x0000FFFFu) == 16u,
          "clz must find the highest set bit within the low half-word");
    Check(ChikaTinyExrCountLeadingZeros32(0x80000000u) == 0u,
          "clz of a value with the top bit set must be zero");
    Check(ChikaTinyExrCountLeadingZeros32(0xFFFFFFFFu) == 0u,
          "clz of all bits set must be zero");
    Check(ChikaTinyExrCountLeadingZeros32(0x00010000u) == 15u,
          "clz must locate an isolated high bit precisely");

    // Macro rewrite checks: TinyEXR's vendored source calls the GCC/Clang
    // builtin name directly, so the #define must transparently forward to
    // the shim with identical results.
    Check(__builtin_clz(1u) == 31u, "the __builtin_clz macro must forward to the shim for value 1");
    Check(__builtin_clz(0x80000000u) == 0u, "the __builtin_clz macro must forward to the shim for the top bit");
    Check(__builtin_clz(0xFFFFFFFFu) == ChikaTinyExrCountLeadingZeros32(0xFFFFFFFFu),
          "the __builtin_clz macro must be equivalent to calling the shim directly");
#else
    Check(true, "TinyExrMsvcCompat.h is only injected on MSVC builds; nothing to verify on this toolchain");
#endif

    return g_failures == 0 ? 0 : 1;
}