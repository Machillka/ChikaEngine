#include "ChikaEngine/scene/UIDGenerator.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include <cstdint>
#include <mutex>

namespace ChikaEngine::Framework
{
    UIDGenerator& UIDGenerator::Instance()
    {
        static UIDGenerator instance;
        return instance;
    }

    void UIDGenerator::Init(const std::uint32_t machineID)
    {
        std::lock_guard lock(_mutex);
        // 防止覆盖变化
        if (_isInitialized)
            return;
        if (machineID > s_MAX_machineID)
        {
            LOG_ERROR("UID Generator", "Machine ID out of range (0-1023)");
            return;
        }

        _machineID = machineID;
        _isInitialized = true;
    }

    GameObjectID UIDGenerator::Generate()
    {
        if (!_isInitialized)
        {
            LOG_ERROR("UID Generator", "Generator haven't been initalized");
            return 0;
        }
        std::lock_guard lock(_mutex);

        std::uint64_t timestamp = GetCurrentTimestamp();
        // 获得的时间比 上次更新时间小 错误!
        if (timestamp < _lastTimestamp)
        {
            // 如果回拨时间很短，可以循环等待；如果很长，说明系统时间故障
            LOG_ERROR("UUIDGenerator", "Clock moved backwards! Rejecting generation.");
            return 0;
        }

        // 同一个时间戳 —— 自增序列号
        if (timestamp == _lastTimestamp)
        {
            _sequence = (_sequence + 1) & s_MAX_sequenceID;
            // 该毫秒达到处理上限 移动到下一毫秒 process
            if (_sequence == 0)
            {
                timestamp = WaitNextMillis(_lastTimestamp);
            }
        }
        else
        {
            _sequence = 0;
        }

        _lastTimestamp = timestamp;
        return ((timestamp - s_epoch) << s_timeShift) | (static_cast<uint64_t>(_machineID) << s_machineShift) | _sequence;
    }

    uint64_t UIDGenerator::GetCurrentTimestamp() const
    {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }

    uint64_t UIDGenerator::WaitNextMillis(uint64_t last) const
    {
        // NOTE: 因为上锁 直接进行一个阻塞
        uint64_t now = GetCurrentTimestamp();
        while (now <= last)
        {
            now = GetCurrentTimestamp();
        }
        return now;
    }
} // namespace ChikaEngine::Framework