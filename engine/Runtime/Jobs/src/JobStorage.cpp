#include "ChikaEngine/jobs/JobStorage.hpp"

#include <algorithm>

namespace ChikaEngine::Jobs
{
    JobStorage::JobStorage(size_t capacity) : m_capacity(std::max<size_t>(1, capacity)), m_slots(std::make_unique<Detail::JobSlot[]>(m_capacity))
    {
        m_freeIndices.reserve(m_capacity);
        for (size_t index = m_capacity; index > 0; --index)
            m_freeIndices.push_back(static_cast<uint32_t>(index - 1));
    }

    JobStorage::~JobStorage() = default;

    JobHandle JobStorage::Allocate(JobDesc desc)
    {
        uint32_t index = JobHandle::InvalidIndex;
        {
            std::lock_guard lock(m_freeMutex);
            if (m_freeIndices.empty())
                return JobHandle::Invalid();
            index = m_freeIndices.back();
            m_freeIndices.pop_back();
        }

        Detail::JobSlot& slot = m_slots[index];
        std::lock_guard slotLock(slot.mutex);
        slot.function = std::move(desc.function);
        slot.parent = JobHandle::Invalid();
        slot.target = desc.target;
        slot.failurePolicy = desc.failurePolicy;
        slot.nameId = desc.nameId;
        slot.enqueueTimestampNs = 0;
        slot.desiredTerminalState = JobState::Completed;
        slot.exception = nullptr;
        slot.dependents.clear();
        slot.remainingDependencies.store(0, std::memory_order_relaxed);
        slot.unfinishedWork.store(1, std::memory_order_relaxed);
        slot.internalReferences.store(0, std::memory_order_relaxed);
        slot.dependencyFailed.store(false, std::memory_order_relaxed);
        slot.ownContributionFinished.store(false, std::memory_order_relaxed);
        slot.setupComplete.store(false, std::memory_order_relaxed);
        slot.finalizing.store(false, std::memory_order_relaxed);
        slot.autoRelease.store(false, std::memory_order_relaxed);
        slot.state.store(JobState::Created, std::memory_order_release);
        m_activeCount.fetch_add(1, std::memory_order_relaxed);
        return { index, slot.generation.load(std::memory_order_acquire) };
    }

    bool JobStorage::Release(JobHandle handle)
    {
        Detail::JobSlot* slot = Resolve(handle);
        if (!slot)
            return false;

        std::unique_lock slotLock(slot->mutex);
        const JobState state = slot->state.load(std::memory_order_acquire);
        const bool terminal = state == JobState::Completed || state == JobState::Cancelled || state == JobState::Failed;
        if (!terminal || slot->internalReferences.load(std::memory_order_acquire) != 0)
            return false;
        slot->function.Reset();
        slot->dependents.clear();
        slot->exception = nullptr;
        slot->state.store(JobState::Free, std::memory_order_release);
        uint32_t nextGeneration = slot->generation.load(std::memory_order_relaxed) + 1u;
        if (nextGeneration == 0)
            nextGeneration = 1;
        slot->generation.store(nextGeneration, std::memory_order_release);
        slotLock.unlock();

        {
            std::lock_guard freeLock(m_freeMutex);
            m_freeIndices.push_back(handle.index);
        }
        m_activeCount.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    bool JobStorage::IsValid(JobHandle handle) const
    {
        return Resolve(handle) != nullptr;
    }

    size_t JobStorage::Capacity() const
    {
        return m_capacity;
    }

    size_t JobStorage::ActiveCount() const
    {
        return m_activeCount.load(std::memory_order_acquire);
    }

    Detail::JobSlot* JobStorage::Resolve(JobHandle handle)
    {
        if (!handle.IsValid() || handle.index >= m_capacity)
            return nullptr;
        Detail::JobSlot& slot = m_slots[handle.index];
        if (slot.generation.load(std::memory_order_acquire) != handle.generation || slot.state.load(std::memory_order_acquire) == JobState::Free)
            return nullptr;
        return &slot;
    }

    const Detail::JobSlot* JobStorage::Resolve(JobHandle handle) const
    {
        if (!handle.IsValid() || handle.index >= m_capacity)
            return nullptr;
        const Detail::JobSlot& slot = m_slots[handle.index];
        if (slot.generation.load(std::memory_order_acquire) != handle.generation || slot.state.load(std::memory_order_acquire) == JobState::Free)
            return nullptr;
        return &slot;
    }
} // namespace ChikaEngine::Jobs
