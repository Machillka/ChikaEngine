#include "ChikaEngine/profiler/ProfilerThreadRegistry.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace ChikaEngine::Profiler
{
    struct ProfilerThreadRegistry::State
    {
        struct Record
        {
            uint32_t id = 0;
            std::string name;
            std::unique_ptr<ProfilerEventBuffer> buffer;
            std::atomic<bool> exited{ false };
        };

        mutable std::mutex mutex;
        std::vector<std::unique_ptr<Record>> records;
        uint32_t nextThreadId = 1;
        size_t defaultCapacity = 65'536;
    };

    namespace
    {
        struct ThreadRegistration
        {
            std::weak_ptr<ProfilerThreadRegistry::State> state;
            ProfilerThreadRegistry::State::Record* record = nullptr;

            ~ThreadRegistration()
            {
                if (record && !state.expired())
                    record->exited.store(true, std::memory_order_release);
            }
        };

        thread_local ThreadRegistration g_registration;

        /** @brief Creates a deterministic fallback name without platform-specific APIs. */
        std::string DefaultThreadName(uint32_t id)
        {
            return id == 1 ? "Main Thread" : "Thread " + std::to_string(id);
        }
    } // namespace

    ProfilerThreadRegistry::ProfilerThreadRegistry(size_t defaultBufferCapacity) : m_state(std::make_shared<State>())
    {
        m_state->defaultCapacity = std::max<size_t>(2, defaultBufferCapacity);
    }

    ProfilerThreadRegistry::~ProfilerThreadRegistry() = default;

    ProfilerEventBuffer* ProfilerThreadRegistry::GetOrRegisterCurrentThread()
    {
        if (const auto owner = g_registration.state.lock(); owner == m_state && g_registration.record)
            return g_registration.record->buffer.get();

        if (g_registration.record)
        {
            if (const auto previous = g_registration.state.lock())
                g_registration.record->exited.store(true, std::memory_order_release);
        }

        std::lock_guard lock(m_state->mutex);
        auto record = std::make_unique<State::Record>();
        record->id = m_state->nextThreadId++;
        record->name = DefaultThreadName(record->id);
        record->buffer = std::make_unique<ProfilerEventBuffer>(m_state->defaultCapacity);
        State::Record* result = record.get();
        m_state->records.push_back(std::move(record));
        g_registration.state = m_state;
        g_registration.record = result;
        return result->buffer.get();
    }

    uint32_t ProfilerThreadRegistry::GetCurrentThreadId()
    {
        GetOrRegisterCurrentThread();
        return g_registration.record->id;
    }

    void ProfilerThreadRegistry::SetCurrentThreadName(std::string_view name)
    {
        GetOrRegisterCurrentThread();
        std::lock_guard lock(m_state->mutex);
        g_registration.record->name = name.empty() ? DefaultThreadName(g_registration.record->id) : std::string(name);
    }

    std::vector<ProfilerThreadBatch> ProfilerThreadRegistry::DrainAll()
    {
        std::vector<State::Record*> records;
        {
            std::lock_guard lock(m_state->mutex);
            records.reserve(m_state->records.size());
            for (const auto& record : m_state->records)
                records.push_back(record.get());
        }

        std::vector<ProfilerThreadBatch> batches;
        batches.reserve(records.size());
        for (State::Record* record : records)
        {
            ProfilerThreadBatch batch;
            batch.threadId = record->id;
            {
                std::lock_guard lock(m_state->mutex);
                batch.threadName = record->name;
            }
            record->buffer->Drain(batch.events);
            batch.droppedEventCount = record->buffer->ConsumeDroppedCount();
            batch.threadExited = record->exited.load(std::memory_order_acquire);
            if (!batch.events.empty() || batch.droppedEventCount > 0 || batch.threadExited)
                batches.push_back(std::move(batch));
        }
        return batches;
    }

    void ProfilerThreadRegistry::SetDefaultBufferCapacity(size_t capacity)
    {
        std::lock_guard lock(m_state->mutex);
        m_state->defaultCapacity = std::max<size_t>(2, capacity);
    }

    size_t ProfilerThreadRegistry::GetRegisteredThreadCount() const
    {
        std::lock_guard lock(m_state->mutex);
        return m_state->records.size();
    }
} // namespace ChikaEngine::Profiler
