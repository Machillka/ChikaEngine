#include "ChikaEngine/PhysicsBodyRegistry.hpp"

#include <atomic>
#include <cstdint>
#include <limits>

namespace ChikaEngine::Physics
{
    namespace
    {
        std::atomic<std::uint64_t> g_nextBodyGeneration{ 1 };

        std::uint32_t AcquireBodyGeneration() noexcept
        {
            const std::uint64_t generation = g_nextBodyGeneration.fetch_add(1, std::memory_order_relaxed);
            if (generation == 0 || generation > std::numeric_limits<std::uint32_t>::max())
                return 0;
            return static_cast<std::uint32_t>(generation);
        }
    } // namespace

    PhysicsBodyHandle PhysicsBodyRegistry::Reserve()
    {
        std::lock_guard lock(_mutex);
        const std::uint32_t generation = AcquireBodyGeneration();
        if (generation == 0)
            return PhysicsBodyHandle::Invalid();

        std::uint32_t index = 0;
        if (!_freeSlots.empty())
        {
            index = _freeSlots.back();
            _freeSlots.pop_back();
        }
        else
        {
            if (_slots.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
                return PhysicsBodyHandle::Invalid();
            index = static_cast<std::uint32_t>(_slots.size());
            _slots.emplace_back();
        }

        Slot& slot = _slots[index];
        slot.generation = generation;
        slot.state = SlotState::Reserved;
        slot.record = {};
        return PhysicsBodyHandle::FromParts(index, generation);
    }

    bool PhysicsBodyRegistry::CancelReservation(PhysicsBodyHandle handle)
    {
        std::lock_guard lock(_mutex);
        if (!handle || handle.Index() >= _slots.size())
            return false;
        Slot& slot = _slots[handle.Index()];
        if (slot.state != SlotState::Reserved || slot.generation != handle.Generation())
            return false;
        ReleaseSlot(handle.Index());
        return true;
    }

    bool PhysicsBodyRegistry::Commit(const PhysicsBodyRecord& record)
    {
        std::lock_guard lock(_mutex);
        if (!record.handle || !record.backendToken || !Core::IsValidGameObjectID(record.ownerId) || record.handle.Index() >= _slots.size() || _ownerToHandle.contains(record.ownerId))
            return false;

        Slot& slot = _slots[record.handle.Index()];
        if (slot.state != SlotState::Reserved || slot.generation != record.handle.Generation())
            return false;

        slot.state = SlotState::Active;
        slot.record = record;
        slot.record.active = true;
        _ownerToHandle[record.ownerId] = record.handle;
        ++_activeCount;
        return true;
    }

    bool PhysicsBodyRegistry::Replace(PhysicsBodyHandle oldHandle, const PhysicsBodyRecord& replacement)
    {
        std::lock_guard lock(_mutex);
        if (!oldHandle || !replacement.handle || !replacement.backendToken || oldHandle.Index() >= _slots.size() || replacement.handle.Index() >= _slots.size())
            return false;

        Slot& oldSlot = _slots[oldHandle.Index()];
        Slot& newSlot = _slots[replacement.handle.Index()];
        if (oldSlot.state != SlotState::Active || oldSlot.generation != oldHandle.Generation() || newSlot.state != SlotState::Reserved || newSlot.generation != replacement.handle.Generation() || oldSlot.record.ownerId != replacement.ownerId)
            return false;

        const auto ownerIt = _ownerToHandle.find(replacement.ownerId);
        if (ownerIt == _ownerToHandle.end() || ownerIt->second != oldHandle)
            return false;

        ReleaseSlot(oldHandle.Index());
        newSlot.state = SlotState::Active;
        newSlot.record = replacement;
        newSlot.record.active = true;
        ownerIt->second = replacement.handle;
        return true;
    }

    std::optional<PhysicsBodyRecord> PhysicsBodyRegistry::Remove(PhysicsBodyHandle handle)
    {
        std::lock_guard lock(_mutex);
        if (!handle || handle.Index() >= _slots.size())
            return std::nullopt;
        Slot& slot = _slots[handle.Index()];
        if (slot.state != SlotState::Active || slot.generation != handle.Generation())
            return std::nullopt;

        PhysicsBodyRecord record = slot.record;
        _ownerToHandle.erase(record.ownerId);
        ReleaseSlot(handle.Index());
        --_activeCount;
        return record;
    }

    std::optional<PhysicsBodyRecord> PhysicsBodyRegistry::Find(PhysicsBodyHandle handle) const
    {
        std::lock_guard lock(_mutex);
        if (!handle || handle.Index() >= _slots.size())
            return std::nullopt;
        const Slot& slot = _slots[handle.Index()];
        if (slot.state != SlotState::Active || slot.generation != handle.Generation())
            return std::nullopt;
        return slot.record;
    }

    std::optional<PhysicsBodyRecord> PhysicsBodyRegistry::FindByOwner(Core::GameObjectID ownerId) const
    {
        std::lock_guard lock(_mutex);
        const auto ownerIt = _ownerToHandle.find(ownerId);
        if (ownerIt == _ownerToHandle.end())
            return std::nullopt;
        const PhysicsBodyHandle handle = ownerIt->second;
        if (!handle || handle.Index() >= _slots.size())
            return std::nullopt;
        const Slot& slot = _slots[handle.Index()];
        if (slot.state != SlotState::Active || slot.generation != handle.Generation())
            return std::nullopt;
        return slot.record;
    }

    std::vector<PhysicsBodyRecord> PhysicsBodyRegistry::SnapshotActive() const
    {
        std::lock_guard lock(_mutex);
        std::vector<PhysicsBodyRecord> records;
        records.reserve(_activeCount);
        for (const Slot& slot : _slots)
        {
            if (slot.state == SlotState::Active)
                records.push_back(slot.record);
        }
        return records;
    }

    std::size_t PhysicsBodyRegistry::ActiveCount() const noexcept
    {
        std::lock_guard lock(_mutex);
        return _activeCount;
    }

    void PhysicsBodyRegistry::Clear() noexcept
    {
        std::lock_guard lock(_mutex);
        _slots.clear();
        _freeSlots.clear();
        _ownerToHandle.clear();
        _activeCount = 0;
    }

    void PhysicsBodyRegistry::ReleaseSlot(std::uint32_t index) noexcept
    {
        Slot& slot = _slots[index];
        slot.generation = 0;
        slot.state = SlotState::Free;
        slot.record = {};
        _freeSlots.push_back(index);
    }
} // namespace ChikaEngine::Physics
