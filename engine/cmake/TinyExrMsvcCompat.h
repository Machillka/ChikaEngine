#pragma once

#include <intrin.h>

#pragma intrinsic(_BitScanReverse)

static __forceinline unsigned int ChikaTinyExrCountLeadingZeros32(unsigned int value)
{
    unsigned long index = 0;
    return _BitScanReverse(&index, value) ? 31u - (unsigned int)index : 32u;
}

#define __builtin_clz(value) ChikaTinyExrCountLeadingZeros32(value)
