#pragma once

#include <cstdint>
#include <mutex>

namespace ChikaEngine::Core
{

    using GameObjectID = std::uint64_t; // 使用 64bits 符合雪花算法
    // TODO: 创建 machine 分配到能力,使得其可以处理多线程多机器
    class UIDGenerator
    {
      public:
        static UIDGenerator& Instance();
        void Init(const std::uint32_t machineID);
        GameObjectID Generate();

      private:
        uint64_t GetCurrentTimestamp() const;
        uint64_t WaitNextMillis(uint64_t last) const;

        UIDGenerator() = default;
        ~UIDGenerator() = default;

        static constexpr std::uint64_t s_epoch = 1770902978469Ull;

        static constexpr int s_machineIDBits = 10;  // 10 bits 工作机器 ID
        static constexpr int s_sequenceIDBits = 12; // 12 bits 吞吐量
        static constexpr std::uint32_t s_MAX_machineID = (1 << s_machineIDBits) - 1;
        static constexpr std::uint32_t s_MAX_sequenceID = (1 << s_sequenceIDBits) - 1;

        static constexpr int s_machineShift = s_machineIDBits; // 偏移量
        static constexpr int s_timeShift = s_machineIDBits + s_sequenceIDBits;

        bool _isInitialized = false;
        std::uint32_t _machineID = 0;
        std::uint32_t _sequence = 0;
        std::uint64_t _lastTimestamp = 0;
        std::mutex _mutex;
    };
} // namespace ChikaEngine::Core