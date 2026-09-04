#pragma once

#include "ChikaEngine/PhysicsDescs.h"

#include <cstddef>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Physics
{
    struct PhysicsBodyRecord
    {
        PhysicsBodyHandle handle = PhysicsBodyHandle::Invalid();
        PhysicsBackendBodyToken backendToken = PhysicsBackendBodyToken::Invalid();
        Core::GameObjectID ownerId = Core::InvalidGameObjectID;
        PhysicsColliderHandle colliderHandle = PhysicsColliderHandle::Invalid();
        MotionType motionType = MotionType::Dynamic;
        bool active = false;
    };

    /** @brief Scene-owned engine Handle registry; backend-native identity remains an opaque token. */
    class PhysicsBodyRegistry
    {
      public:
        [[nodiscard]] PhysicsBodyHandle Reserve();
        [[nodiscard]] bool CancelReservation(PhysicsBodyHandle handle);
        [[nodiscard]] bool Commit(const PhysicsBodyRecord& record);
        [[nodiscard]] bool Replace(PhysicsBodyHandle oldHandle, const PhysicsBodyRecord& replacement);
        [[nodiscard]] std::optional<PhysicsBodyRecord> Remove(PhysicsBodyHandle handle);

        [[nodiscard]] std::optional<PhysicsBodyRecord> Find(PhysicsBodyHandle handle) const;
        [[nodiscard]] std::optional<PhysicsBodyRecord> FindByOwner(Core::GameObjectID ownerId) const;
        [[nodiscard]] std::vector<PhysicsBodyRecord> SnapshotActive() const;
        [[nodiscard]] std::size_t ActiveCount() const noexcept;
        void Clear() noexcept;

      private:
        enum class SlotState
        {
            Free,
            Reserved,
            Active,
        };

        struct Slot
        {
            std::uint32_t generation = 0;
            SlotState state = SlotState::Free;
            PhysicsBodyRecord record;
        };

        void ReleaseSlot(std::uint32_t index) noexcept;

        mutable std::mutex _mutex;
        std::vector<Slot> _slots;
        std::vector<std::uint32_t> _freeSlots;
        std::unordered_map<Core::GameObjectID, PhysicsBodyHandle> _ownerToHandle;
        std::size_t _activeCount = 0;
    };
} // namespace ChikaEngine::Physics
