#pragma once

#include <cstdint>
namespace ChikaEngine::Core
{
    template <typename Tag> struct THandle
    {
        static constexpr std::uint32_t INDEX_BITS = 20;
        // 低位 20 bits
        static constexpr std::uint32_t INDEX_MASK = (1u << INDEX_BITS) - 1;
        static constexpr std::uint32_t GEN_MASK = ~INDEX_MASK;
        static constexpr std::uint32_t GEN_SHIFT = INDEX_BITS;

        // 非法
        static constexpr std::uint32_t INVALID_RAW = 0xFFFFFFFF;

        std::uint32_t raw_value;

        // 默认构造 赋值非法
        THandle() : raw_value(INVALID_RAW) {}
        explicit THandle(std::uint32_t in_raw) : raw_value(in_raw) {}

        static THandle FromParts(std::uint32_t index, std::uint32_t gen)
        {
            return THandle((index & INDEX_MASK) | (gen << GEN_SHIFT));
        }

        std::uint32_t GetIndex() const
        {
            return raw_value & INDEX_MASK;
        }
        std::uint32_t GetGen() const
        {
            return raw_value >> GEN_SHIFT;
        }

        bool IsValid() const
        {
            return raw_value != INVALID_RAW && GetIndex() != INDEX_MASK;
        }
        static THandle Invalid()
        {
            return THandle(INVALID_RAW);
        }

        bool operator!() const
        {
            return !IsValid();
        }
        bool operator==(const THandle& o) const
        {
            return raw_value == o.raw_value;
        }
        bool operator!=(const THandle& o) const
        {
            return raw_value != o.raw_value;
        }
        bool operator<(const THandle& o) const
        {
            return raw_value < o.raw_value;
        }

        explicit operator std::uint32_t() const
        {
            return raw_value;
        }
    };
} // namespace ChikaEngine::Core