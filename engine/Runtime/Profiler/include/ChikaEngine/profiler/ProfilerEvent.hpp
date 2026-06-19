#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>

namespace ChikaEngine::Profiler
{
    inline constexpr uint32_t kProfilerEventSchemaVersion = 1;
    inline constexpr uint32_t kInvalidProfilerNameId = 0;
    inline constexpr uint64_t kInvalidProfilerFrameId = UINT64_MAX;

    enum class ProfilerEventType : uint8_t
    {
        FrameMark,
        ZoneBegin,
        ZoneEnd,
        Counter,
        Instant,
        ThreadName,
    };

    enum class ProfilerEventFlags : uint8_t
    {
        None = 0,
        FrameBegin = 1u << 0u,
        FloatingPointPayload = 1u << 1u,
    };

    /**
     * @brief Fixed-size event written by one producer thread and drained by the aggregator.
     *
     * Events contain IDs and scalar payloads only. Keeping dynamic strings out of the hot path
     * makes the record trivially copyable and safe to move between thread buffers.
     */
    struct alignas(8) ProfilerEvent
    {
        uint64_t timestampNs = 0;
        uint64_t frameId = kInvalidProfilerFrameId;
        uint64_t payload = 0;
        uint32_t threadId = 0;
        uint32_t nameId = kInvalidProfilerNameId;
        ProfilerEventType type = ProfilerEventType::Instant;
        ProfilerEventFlags flags = ProfilerEventFlags::None;
        uint16_t reserved = 0;
    };

    static_assert(std::is_trivially_copyable_v<ProfilerEvent>);
    static_assert(sizeof(ProfilerEvent) == 40);

    /** @brief Packs a signed counter without allocating or changing its bit representation. */
    inline uint64_t PackProfilerInteger(int64_t value)
    {
        return std::bit_cast<uint64_t>(value);
    }

    /** @brief Packs a floating counter so event payload remains a fixed 64-bit scalar. */
    inline uint64_t PackProfilerDouble(double value)
    {
        return std::bit_cast<uint64_t>(value);
    }

    inline int64_t UnpackProfilerInteger(uint64_t value)
    {
        return std::bit_cast<int64_t>(value);
    }

    inline double UnpackProfilerDouble(uint64_t value)
    {
        return std::bit_cast<double>(value);
    }
} // namespace ChikaEngine::Profiler
