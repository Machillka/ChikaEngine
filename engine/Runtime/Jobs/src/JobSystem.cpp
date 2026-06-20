#include "ChikaEngine/jobs/JobSystem.hpp"

#include "ChikaEngine/jobs/JobProfiler.hpp"
#include "ChikaEngine/jobs/JobQueue.hpp"
#include "ChikaEngine/jobs/JobStorage.hpp"
#include "ChikaEngine/profiler/ProfilerClock.hpp"
#include "ChikaEngine/profiler/ProfilerName.hpp"
#include "ChikaEngine/profiler/ProfilerSession.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace ChikaEngine::Jobs
{
    namespace
    {
        constexpr uint32_t kExternalWorkerIndex = UINT32_MAX;
        thread_local void* g_scheduler = nullptr;
        thread_local uint32_t g_workerIndex = kExternalWorkerIndex;
        thread_local JobHandle g_currentJob;

        /** @brief Keeps terminal-state checks consistent across wait, release, and dependency paths. */
        bool IsTerminal(JobState state)
        {
            return state == JobState::Completed || state == JobState::Cancelled || state == JobState::Failed;
        }
    } // namespace

    class JobSystem::Impl
    {
      public:
        /** @brief Builds all bounded scheduler storage before publishing initialized state. */
        bool Initialize(const JobSystemCreateInfo& createInfo)
        {
            if (m_initialized.load(std::memory_order_acquire))
                return true;

            m_createInfo = createInfo;
            m_workerCount = JobSystem::ResolveWorkerCount(createInfo.workerCount, createInfo.reservedThreads);
            m_storage = std::make_unique<JobStorage>(createInfo.jobCapacity);
            m_injectionQueue = std::make_unique<JobQueue>(createInfo.injectionQueueCapacity);
            m_mainThreadQueue = std::make_unique<JobQueue>(createInfo.injectionQueueCapacity);
            m_localQueues.clear();
            m_localQueues.reserve(m_workerCount);
            for (uint32_t index = 0; index < m_workerCount; ++index)
                m_localQueues.push_back(std::make_unique<JobQueue>(createInfo.localQueueCapacity));

            m_mainThreadId = std::this_thread::get_id();
            m_stopRequested.store(false, std::memory_order_release);
            m_acceptingWorkerSubmissions.store(false, std::memory_order_release);
            m_accepting.store(true, std::memory_order_release);
            m_initializedAtNs = Profiler::ProfilerClock::NowNanoseconds();
            ResetStatistics();

            try
            {
                for (uint32_t workerIndex = 0; workerIndex < m_workerCount; ++workerIndex)
                {
                    if (workerIndex == createInfo.failWorkerStartAt)
                        throw std::runtime_error("injected worker startup failure");
                    m_workers.emplace_back([this, workerIndex]() { WorkerMain(workerIndex); });
                }
            }
            catch (...)
            {
                m_accepting.store(false, std::memory_order_release);
                m_stopRequested.store(true, std::memory_order_release);
                m_wakeCondition.notify_all();
                for (std::thread& worker : m_workers)
                {
                    if (worker.joinable())
                        worker.join();
                }
                m_workers.clear();
                ResetRuntimeStorage();
                return false;
            }

            m_initialized.store(true, std::memory_order_release);
            return true;
        }

        /** @brief Closes external submission, resolves accepted work, then joins every worker. */
        void Shutdown(JobShutdownPolicy policy)
        {
            if (!m_storage)
                return;
            {
                std::unique_lock lifecycleLock(m_lifecycleMutex);
                m_accepting.store(false, std::memory_order_release);
                m_acceptingWorkerSubmissions.store(policy == JobShutdownPolicy::Drain, std::memory_order_release);
            }
            if (policy == JobShutdownPolicy::CancelPending)
            {
                m_storage->ForEachActive(
                    [this](JobHandle handle, Detail::JobSlot& slot)
                    {
                        const JobState state = slot.state.load(std::memory_order_acquire);
                        if (state == JobState::Created || state == JobState::Queued)
                            Cancel(handle, JobState::Cancelled, std::make_exception_ptr(JobCancelledError()));
                    });
            }

            while (m_outstandingJobs.load(std::memory_order_acquire) != 0)
            {
                PumpMainThreadJobs(64);
                JobHandle helperJob;
                if (TryTakeJob(kExternalWorkerIndex, helperJob))
                    Execute(helperJob);
                else
                {
                    std::unique_lock lock(m_completionMutex);
                    m_completionCondition.wait_for(lock, std::chrono::milliseconds(1), [this] { return m_outstandingJobs.load(std::memory_order_acquire) == 0; });
                }
            }

            std::unique_lock finalLifecycleLock(m_lifecycleMutex);
            m_acceptingWorkerSubmissions.store(false, std::memory_order_release);

            {
                std::unique_lock lock(m_completionMutex);
                m_completionCondition.wait(lock, [this] { return m_activeWaiters.load(std::memory_order_acquire) == 0; });
            }

            m_stopRequested.store(true, std::memory_order_release);
            m_wakeCondition.notify_all();
            for (std::thread& worker : m_workers)
            {
                if (worker.joinable())
                    worker.join();
            }
            m_workers.clear();
            m_initialized.store(false, std::memory_order_release);
            ResetRuntimeStorage();
        }

        JobHandle Schedule(JobDesc desc)
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            return ScheduleInternal({}, JobHandle::Invalid(), std::move(desc));
        }

        JobHandle ScheduleAfter(std::span<const JobHandle> dependencies, JobDesc desc)
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            for (JobHandle dependency : dependencies)
            {
                if (!m_storage || !m_storage->IsValid(dependency))
                    throw InvalidJobHandleError();
            }
            return ScheduleInternal(dependencies, JobHandle::Invalid(), std::move(desc));
        }

        JobHandle ScheduleChild(JobHandle parent, JobDesc desc)
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            Detail::JobSlot* parentSlot = m_storage ? m_storage->Resolve(parent) : nullptr;
            if (!parentSlot)
                throw InvalidJobHandleError();
            {
                std::lock_guard lock(parentSlot->mutex);
                if (IsTerminal(parentSlot->state.load(std::memory_order_acquire)))
                    throw InvalidJobHandleError();
                // The parent mutex serializes child registration against finalization; release occurs on child completion.
                parentSlot->unfinishedWork.fetch_add(1, std::memory_order_relaxed);
                parentSlot->internalReferences.fetch_add(1, std::memory_order_relaxed);
            }

            JobHandle child = ScheduleInternal({}, parent, std::move(desc));
            if (!child.IsValid())
            {
                if (parentSlot->unfinishedWork.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    Finalize(parent, *parentSlot);
                parentSlot->internalReferences.fetch_sub(1, std::memory_order_acq_rel);
            }
            return child;
        }

        void Wait(JobHandle handle)
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            Detail::JobSlot* slot = ResolveOrThrow(handle);
            if (g_scheduler == this && g_currentJob == handle)
                throw std::logic_error("a job cannot wait on itself");

            struct WaiterGuard
            {
                Impl& owner;
                explicit WaiterGuard(Impl& value) : owner(value)
                {
                    owner.m_activeWaiters.fetch_add(1, std::memory_order_relaxed);
                }
                ~WaiterGuard()
                {
                    owner.m_activeWaiters.fetch_sub(1, std::memory_order_acq_rel);
                    owner.m_completionCondition.notify_all();
                }
            } waiterGuard(*this);
            lifecycleLock.unlock();

            const bool canHelp = g_scheduler == this || std::this_thread::get_id() == m_mainThreadId;
            if (canHelp)
            {
                while (!IsComplete(handle))
                {
                    if (std::this_thread::get_id() == m_mainThreadId)
                        PumpMainThreadJobs(16);
                    JobHandle helperJob;
                    if (TryTakeJob(g_scheduler == this ? g_workerIndex : kExternalWorkerIndex, helperJob))
                        Execute(helperJob);
                    else
                    {
                        std::unique_lock lock(slot->mutex);
                        slot->condition.wait_for(lock, std::chrono::microseconds(200), [&] { return IsTerminal(slot->state.load(std::memory_order_acquire)); });
                    }
                    slot = ResolveOrThrow(handle);
                }
            }
            else
            {
                std::unique_lock lock(slot->mutex);
                slot->condition.wait(lock, [&] { return IsTerminal(slot->state.load(std::memory_order_acquire)); });
            }
            RethrowResult(handle);
        }

        bool IsComplete(JobHandle handle) const
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            const Detail::JobSlot* slot = m_storage ? m_storage->Resolve(handle) : nullptr;
            return slot && IsTerminal(slot->state.load(std::memory_order_acquire));
        }

        JobState GetState(JobHandle handle) const
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            const Detail::JobSlot* slot = m_storage ? m_storage->Resolve(handle) : nullptr;
            if (!slot)
                throw InvalidJobHandleError();
            return slot->state.load(std::memory_order_acquire);
        }

        bool Release(JobHandle handle)
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            return m_storage && m_storage->Release(handle);
        }

        bool Detach(JobHandle handle)
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            Detail::JobSlot* slot = m_storage ? m_storage->Resolve(handle) : nullptr;
            if (!slot)
                return false;
            slot->autoRelease.store(true, std::memory_order_release);
            TryAutoRelease(handle, *slot);
            return true;
        }

        size_t PumpMainThreadJobs(size_t maximumJobs)
        {
            std::shared_lock lifecycleLock(m_lifecycleMutex);
            size_t executed = 0;
            JobHandle handle;
            while (executed < maximumJobs && m_mainThreadQueue && m_mainThreadQueue->Steal(handle))
            {
                m_mainReady.fetch_sub(1, std::memory_order_relaxed);
                JobProfiler::QueueDepth(m_readyJobs.load(std::memory_order_relaxed) + m_mainReady.load(std::memory_order_relaxed));
                Execute(handle);
                ++executed;
            }
            return executed;
        }

        JobSystemStatistics GetStatistics() const
        {
            JobSystemStatistics result;
            result.submittedJobs = m_submitted.load(std::memory_order_relaxed);
            result.completedJobs = m_completed.load(std::memory_order_relaxed);
            result.failedJobs = m_failed.load(std::memory_order_relaxed);
            result.cancelledJobs = m_cancelled.load(std::memory_order_relaxed);
            result.localPops = m_localPops.load(std::memory_order_relaxed);
            result.injectionPops = m_injectionPops.load(std::memory_order_relaxed);
            result.stealAttempts = m_stealAttempts.load(std::memory_order_relaxed);
            result.successfulSteals = m_successfulSteals.load(std::memory_order_relaxed);
            result.sleepCount = m_sleepCount.load(std::memory_order_relaxed);
            result.queueWaitNanoseconds = m_queueWaitNs.load(std::memory_order_relaxed);
            result.executionNanoseconds = m_executionNs.load(std::memory_order_relaxed);
            result.activeWorkers = m_activeWorkers.load(std::memory_order_relaxed);
            result.sleepingWorkers = m_sleepingWorkers.load(std::memory_order_relaxed);
            result.workerCount = m_workerCount;
            result.queuedJobs = m_readyJobs.load(std::memory_order_relaxed) + m_mainReady.load(std::memory_order_relaxed);
            const uint64_t elapsed = Profiler::ProfilerClock::NowNanoseconds() - m_initializedAtNs;
            if (elapsed > 0 && m_workerCount > 0)
                result.workerUtilization = std::min(100.0, static_cast<double>(result.executionNanoseconds) / static_cast<double>(elapsed * m_workerCount) * 100.0);
            return result;
        }

        bool IsInitialized() const
        {
            return m_initialized.load(std::memory_order_acquire);
        }

        bool IsAcceptingJobs() const
        {
            return m_accepting.load(std::memory_order_acquire);
        }

        uint32_t GetWorkerCount() const
        {
            return m_workerCount;
        }

        const JobSystemCreateInfo& GetCreateInfo() const
        {
            return m_createInfo;
        }

      private:
        /** @brief Claims a slot and registers dependencies before making the job runnable. */
        JobHandle ScheduleInternal(std::span<const JobHandle> dependencies, JobHandle parent, JobDesc desc)
        {
            const bool internalSubmission = g_scheduler == this && g_currentJob.IsValid();
            if ((!m_accepting.load(std::memory_order_acquire) && !(internalSubmission && m_acceptingWorkerSubmissions.load(std::memory_order_acquire))) || !m_storage)
                return JobHandle::Invalid();
            JobHandle handle = m_storage->Allocate(std::move(desc));
            if (!handle.IsValid())
                return handle;
            Detail::JobSlot* slot = m_storage->Resolve(handle);
            slot->parent = parent;
            slot->remainingDependencies.store(static_cast<uint32_t>(dependencies.size()), std::memory_order_relaxed);
            m_outstandingJobs.fetch_add(1, std::memory_order_relaxed);
            m_submitted.fetch_add(1, std::memory_order_relaxed);

            for (JobHandle dependency : dependencies)
            {
                Detail::JobSlot* dependencySlot = m_storage->Resolve(dependency);
                std::exception_ptr dependencyException;
                JobState dependencyState = JobState::Free;
                bool alreadyComplete = false;
                {
                    std::lock_guard dependencyLock(dependencySlot->mutex);
                    dependencyState = dependencySlot->state.load(std::memory_order_acquire);
                    if (IsTerminal(dependencyState) || dependencySlot->finalizing.load(std::memory_order_acquire))
                    {
                        alreadyComplete = true;
                        if (!IsTerminal(dependencyState))
                            dependencyState = dependencySlot->desiredTerminalState;
                        dependencyException = dependencySlot->exception;
                    }
                    else
                    {
                        dependencySlot->internalReferences.fetch_add(1, std::memory_order_relaxed);
                        dependencySlot->dependents.push_back(handle);
                    }
                }
                if (alreadyComplete)
                    ResolveDependency(handle, dependencyState, dependencyException);
            }

            slot->setupComplete.store(true, std::memory_order_release);
            if (slot->remainingDependencies.load(std::memory_order_acquire) == 0)
                MakeReadyOrPropagate(handle, *slot);
            return handle;
        }

        /** @brief Publishes Created work to the target queue and wakes a worker when required. */
        bool Enqueue(JobHandle handle, Detail::JobSlot& slot)
        {
            JobState expected = JobState::Created;
            if (!slot.state.compare_exchange_strong(expected, JobState::Queued, std::memory_order_acq_rel))
                return false;
            slot.enqueueTimestampNs = Profiler::ProfilerClock::NowNanoseconds();

            bool queued = false;
            if (slot.target == JobTarget::MainThread)
            {
                queued = m_mainThreadQueue->Push(handle);
                if (queued)
                    m_mainReady.fetch_add(1, std::memory_order_relaxed);
            }
            else if (g_scheduler == this && g_workerIndex < m_localQueues.size())
            {
                queued = m_localQueues[g_workerIndex]->Push(handle);
                if (!queued)
                    queued = m_injectionQueue->Push(handle);
            }
            else
                queued = m_injectionQueue->Push(handle);

            if (!queued)
            {
                slot.state.store(JobState::Created, std::memory_order_release);
                Cancel(handle, JobState::Cancelled, std::make_exception_ptr(JobCapacityError("job queue capacity exhausted")));
                return false;
            }

            if (slot.target == JobTarget::AnyWorker)
            {
                m_readyJobs.fetch_add(1, std::memory_order_relaxed);
                m_wakeCondition.notify_one();
            }
            JobProfiler::Enqueued(handle);
            JobProfiler::QueueDepth(m_readyJobs.load(std::memory_order_relaxed) + m_mainReady.load(std::memory_order_relaxed));
            return true;
        }

        /** @brief Applies local, injection, then deterministic steal priority for one helper thread. */
        bool TryTakeJob(uint32_t workerIndex, JobHandle& handle)
        {
            if (workerIndex < m_localQueues.size() && m_localQueues[workerIndex]->PopLocal(handle))
            {
                m_readyJobs.fetch_sub(1, std::memory_order_relaxed);
                m_localPops.fetch_add(1, std::memory_order_relaxed);
                JobProfiler::QueueDepth(m_readyJobs.load(std::memory_order_relaxed) + m_mainReady.load(std::memory_order_relaxed));
                return true;
            }
            if (m_injectionQueue && m_injectionQueue->Steal(handle))
            {
                m_readyJobs.fetch_sub(1, std::memory_order_relaxed);
                m_injectionPops.fetch_add(1, std::memory_order_relaxed);
                JobProfiler::QueueDepth(m_readyJobs.load(std::memory_order_relaxed) + m_mainReady.load(std::memory_order_relaxed));
                return true;
            }
            if (m_localQueues.empty())
                return false;

            const uint32_t start = workerIndex == kExternalWorkerIndex ? m_createInfo.stealSeed % m_workerCount : (workerIndex + 1u + m_createInfo.stealSeed) % m_workerCount;
            for (uint32_t offset = 0; offset < m_workerCount; ++offset)
            {
                const uint32_t victim = (start + offset) % m_workerCount;
                if (victim == workerIndex)
                    continue;
                m_stealAttempts.fetch_add(1, std::memory_order_relaxed);
                if (m_localQueues[victim]->Steal(handle))
                {
                    m_readyJobs.fetch_sub(1, std::memory_order_relaxed);
                    const uint64_t stealCount = m_successfulSteals.fetch_add(1, std::memory_order_relaxed) + 1u;
                    JobProfiler::Stolen(handle);
                    JobProfiler::StealCount(stealCount);
                    JobProfiler::QueueDepth(m_readyJobs.load(std::memory_order_relaxed) + m_mainReady.load(std::memory_order_relaxed));
                    return true;
                }
            }
            return false;
        }

        /** @brief Runs one named worker until shutdown, sleeping on an observable ready-work predicate. */
        void WorkerMain(uint32_t workerIndex)
        {
            g_scheduler = this;
            g_workerIndex = workerIndex;
            Profiler::ProfilerSession::Get().SetCurrentThreadName("Job Worker " + std::to_string(workerIndex));
            while (!m_stopRequested.load(std::memory_order_acquire))
            {
                JobHandle handle;
                if (TryTakeJob(workerIndex, handle))
                {
                    Execute(handle);
                    continue;
                }

                m_sleepCount.fetch_add(1, std::memory_order_relaxed);
                const uint32_t sleeping = m_sleepingWorkers.fetch_add(1, std::memory_order_relaxed) + 1u;
                JobProfiler::SleepingWorkers(sleeping);
                std::unique_lock lock(m_wakeMutex);
                m_wakeCondition.wait(lock, [this] { return m_stopRequested.load(std::memory_order_acquire) || m_readyJobs.load(std::memory_order_acquire) > 0; });
                const uint32_t sleepingAfterWake = m_sleepingWorkers.fetch_sub(1, std::memory_order_relaxed) - 1u;
                JobProfiler::SleepingWorkers(sleepingAfterWake);
            }
            g_currentJob = JobHandle::Invalid();
            g_workerIndex = kExternalWorkerIndex;
            g_scheduler = nullptr;
        }

        /** @brief Claims a queued callable exactly once and converts exceptions into job state. */
        void Execute(JobHandle handle)
        {
            Detail::JobSlot* slot = m_storage ? m_storage->Resolve(handle) : nullptr;
            if (!slot)
                return;
            JobState expected = JobState::Queued;
            if (!slot->state.compare_exchange_strong(expected, JobState::Running, std::memory_order_acq_rel))
                return;

            const uint64_t startNs = Profiler::ProfilerClock::NowNanoseconds();
            const uint64_t queueWait = startNs >= slot->enqueueTimestampNs ? startNs - slot->enqueueTimestampNs : 0;
            m_queueWaitNs.fetch_add(queueWait, std::memory_order_relaxed);
            JobProfiler::QueueWait(queueWait);
            const uint32_t active = m_activeWorkers.fetch_add(1, std::memory_order_relaxed) + 1u;
            JobProfiler::ActiveWorkers(active);
            JobProfiler::Started(handle);
            const bool profileZone = Profiler::ProfilerSession::Get().BeginZone(slot->nameId);
            void* previousScheduler = g_scheduler;
            const uint32_t previousWorkerIndex = g_workerIndex;
            const JobHandle previousJob = g_currentJob;
            if (!g_scheduler)
            {
                g_scheduler = this;
                g_workerIndex = kExternalWorkerIndex;
            }
            g_currentJob = handle;

            JobState terminal = JobState::Completed;
            std::exception_ptr exception;
            try
            {
                if (slot->function)
                    slot->function();
            }
            catch (...)
            {
                terminal = JobState::Failed;
                exception = std::current_exception();
            }

            g_currentJob = previousJob;
            g_workerIndex = previousWorkerIndex;
            g_scheduler = previousScheduler;
            if (profileZone)
                Profiler::ProfilerSession::Get().EndZone(slot->nameId);
            m_executionNs.fetch_add(Profiler::ProfilerClock::NowNanoseconds() - startNs, std::memory_order_relaxed);
            const uint32_t activeAfterFinish = m_activeWorkers.fetch_sub(1, std::memory_order_relaxed) - 1u;
            JobProfiler::ActiveWorkers(activeAfterFinish);
            FinishOwnContribution(handle, terminal, exception);
        }

        /** @brief Removes the callable's own completion contribution while children may remain. */
        void FinishOwnContribution(JobHandle handle, JobState terminal, std::exception_ptr exception)
        {
            Detail::JobSlot* slot = m_storage->Resolve(handle);
            if (!slot || slot->ownContributionFinished.exchange(true, std::memory_order_acq_rel))
                return;
            {
                std::lock_guard lock(slot->mutex);
                if (terminal == JobState::Failed)
                {
                    slot->desiredTerminalState = JobState::Failed;
                    slot->exception = exception;
                }
                else if (slot->desiredTerminalState == JobState::Completed)
                    slot->desiredTerminalState = terminal;
            }
            if (slot->unfinishedWork.fetch_sub(1, std::memory_order_acq_rel) == 1)
                Finalize(handle, *slot);
        }

        /** @brief Publishes dependencies and counters before exposing a releasable terminal state. */
        void Finalize(JobHandle handle, Detail::JobSlot& slot)
        {
            std::vector<JobHandle> dependents;
            JobHandle parent;
            JobState terminal;
            std::exception_ptr exception;
            {
                std::lock_guard lock(slot.mutex);
                if (IsTerminal(slot.state.load(std::memory_order_acquire)) || slot.finalizing.exchange(true, std::memory_order_acq_rel))
                    return;
                terminal = slot.desiredTerminalState;
                exception = slot.exception;
                parent = slot.parent;
                dependents.swap(slot.dependents);
            }

            if (terminal == JobState::Completed)
            {
                m_completed.fetch_add(1, std::memory_order_relaxed);
                JobProfiler::Completed(handle);
            }
            else if (terminal == JobState::Failed)
            {
                m_failed.fetch_add(1, std::memory_order_relaxed);
                JobProfiler::Failed(handle);
            }
            else
            {
                m_cancelled.fetch_add(1, std::memory_order_relaxed);
                JobProfiler::Cancelled(handle);
            }

            for (JobHandle dependent : dependents)
            {
                ResolveDependency(dependent, terminal, exception);
                slot.internalReferences.fetch_sub(1, std::memory_order_acq_rel);
            }
            if (parent.IsValid())
                ResolveChild(parent, terminal, exception);

            m_outstandingJobs.fetch_sub(1, std::memory_order_acq_rel);
            m_completionCondition.notify_all();

            // Terminal state is the public release barrier: after this store, Finalize no longer touches the slot.
            const bool autoRelease = slot.autoRelease.load(std::memory_order_acquire) && slot.internalReferences.load(std::memory_order_acquire) == 0;
            slot.state.store(terminal, std::memory_order_release);
            slot.condition.notify_all();
            if (autoRelease)
                m_storage->Release(handle);
        }

        /** @brief Releases one dependency edge and schedules the continuation on the final edge. */
        void ResolveDependency(JobHandle dependent, JobState dependencyState, std::exception_ptr exception)
        {
            Detail::JobSlot* slot = m_storage->Resolve(dependent);
            if (!slot || slot->state.load(std::memory_order_acquire) != JobState::Created)
                return;
            if (dependencyState != JobState::Completed)
            {
                slot->dependencyFailed.store(true, std::memory_order_release);
                std::lock_guard lock(slot->mutex);
                if (!slot->exception)
                    slot->exception = exception ? exception : std::make_exception_ptr(JobCancelledError());
            }
            // acq_rel publishes predecessor writes to the thread that observes and enqueues the final edge.
            const uint32_t previous = slot->remainingDependencies.fetch_sub(1, std::memory_order_acq_rel);
            if (previous == 1 && slot->setupComplete.load(std::memory_order_acquire))
                MakeReadyOrPropagate(dependent, *slot);
        }

        /** @brief Converts dependency outcome and failure policy into runnable or terminal state. */
        void MakeReadyOrPropagate(JobHandle handle, Detail::JobSlot& slot)
        {
            if (!slot.dependencyFailed.load(std::memory_order_acquire) || slot.failurePolicy == JobFailurePolicy::RunAnyway)
            {
                Enqueue(handle, slot);
                return;
            }
            const JobState terminal = slot.failurePolicy == JobFailurePolicy::Propagate ? JobState::Failed : JobState::Cancelled;
            Cancel(handle, terminal, slot.exception);
        }

        /** @brief Publishes child outcome into its parent completion counter and internal reference. */
        void ResolveChild(JobHandle parent, JobState childState, std::exception_ptr exception)
        {
            Detail::JobSlot* parentSlot = m_storage->Resolve(parent);
            if (!parentSlot)
                return;
            if (childState != JobState::Completed)
            {
                std::lock_guard lock(parentSlot->mutex);
                parentSlot->desiredTerminalState = JobState::Failed;
                if (!parentSlot->exception)
                    parentSlot->exception = exception ? exception : std::make_exception_ptr(JobCancelledError());
            }
            const bool autoRelease = parentSlot->autoRelease.load(std::memory_order_acquire);
            if (parentSlot->unfinishedWork.fetch_sub(1, std::memory_order_acq_rel) == 1)
                Finalize(parent, *parentSlot);
            const uint32_t previousReferences = parentSlot->internalReferences.fetch_sub(1, std::memory_order_acq_rel);
            if (autoRelease && previousReferences == 1)
                m_storage->Release(parent);
        }

        /** @brief Claims non-running work and completes it without invoking the callable. */
        bool Cancel(JobHandle handle, JobState terminal, std::exception_ptr exception)
        {
            Detail::JobSlot* slot = m_storage->Resolve(handle);
            if (!slot)
                return false;
            JobState state = slot->state.load(std::memory_order_acquire);
            while (state == JobState::Created || state == JobState::Queued)
            {
                if (slot->state.compare_exchange_weak(state, JobState::Running, std::memory_order_acq_rel))
                {
                    {
                        std::lock_guard lock(slot->mutex);
                        slot->desiredTerminalState = terminal;
                        slot->exception = exception;
                    }
                    FinishOwnContribution(handle, terminal, exception);
                    return true;
                }
            }
            return false;
        }

        /** @brief Recycles detached terminal work after dependency-owned references are gone. */
        void TryAutoRelease(JobHandle handle, Detail::JobSlot& slot)
        {
            if (slot.autoRelease.load(std::memory_order_acquire) && IsTerminal(slot.state.load(std::memory_order_acquire)) && slot.internalReferences.load(std::memory_order_acquire) == 0)
                m_storage->Release(handle);
        }

        /** @brief Reconstructs Wait semantics from the stored terminal state and exception. */
        void RethrowResult(JobHandle handle)
        {
            Detail::JobSlot* slot = ResolveOrThrow(handle);
            const JobState state = slot->state.load(std::memory_order_acquire);
            if (state == JobState::Failed)
            {
                std::exception_ptr exception;
                {
                    std::lock_guard lock(slot->mutex);
                    exception = slot->exception;
                }
                if (exception)
                    std::rethrow_exception(exception);
                throw std::runtime_error("job failed without exception payload");
            }
            if (state == JobState::Cancelled)
                throw JobCancelledError();
        }

        /** @brief Centralizes stale generation rejection for handle-consuming APIs. */
        Detail::JobSlot* ResolveOrThrow(JobHandle handle) const
        {
            Detail::JobSlot* slot = m_storage ? m_storage->Resolve(handle) : nullptr;
            if (!slot)
                throw InvalidJobHandleError();
            return slot;
        }

        /** @brief Clears counters on every fresh initialization so runs remain independent. */
        void ResetStatistics()
        {
            m_submitted.store(0);
            m_completed.store(0);
            m_failed.store(0);
            m_cancelled.store(0);
            m_localPops.store(0);
            m_injectionPops.store(0);
            m_stealAttempts.store(0);
            m_successfulSteals.store(0);
            m_sleepCount.store(0);
            m_queueWaitNs.store(0);
            m_executionNs.store(0);
            m_activeWorkers.store(0);
            m_sleepingWorkers.store(0);
            m_readyJobs.store(0);
            m_mainReady.store(0);
            m_outstandingJobs.store(0);
            m_activeWaiters.store(0);
        }

        /** @brief Releases queues and slots only after workers and waiters have stopped using them. */
        void ResetRuntimeStorage()
        {
            m_localQueues.clear();
            m_injectionQueue.reset();
            m_mainThreadQueue.reset();
            m_storage.reset();
            m_workerCount = 0;
            m_readyJobs.store(0);
            m_mainReady.store(0);
        }

        JobSystemCreateInfo m_createInfo;
        std::unique_ptr<JobStorage> m_storage;
        std::unique_ptr<JobQueue> m_injectionQueue;
        std::unique_ptr<JobQueue> m_mainThreadQueue;
        std::vector<std::unique_ptr<JobQueue>> m_localQueues;
        std::vector<std::thread> m_workers;
        std::thread::id m_mainThreadId;
        uint32_t m_workerCount = 0;
        uint64_t m_initializedAtNs = 0;
        std::atomic<bool> m_initialized{ false };
        std::atomic<bool> m_accepting{ false };
        std::atomic<bool> m_acceptingWorkerSubmissions{ false };
        std::atomic<bool> m_stopRequested{ false };
        std::atomic<uint32_t> m_readyJobs{ 0 };
        std::atomic<uint32_t> m_mainReady{ 0 };
        std::atomic<uint64_t> m_outstandingJobs{ 0 };
        std::atomic<uint32_t> m_activeWaiters{ 0 };
        mutable std::shared_mutex m_lifecycleMutex;
        std::mutex m_wakeMutex;
        std::condition_variable m_wakeCondition;
        std::mutex m_completionMutex;
        std::condition_variable m_completionCondition;

        std::atomic<uint64_t> m_submitted{ 0 };
        std::atomic<uint64_t> m_completed{ 0 };
        std::atomic<uint64_t> m_failed{ 0 };
        std::atomic<uint64_t> m_cancelled{ 0 };
        std::atomic<uint64_t> m_localPops{ 0 };
        std::atomic<uint64_t> m_injectionPops{ 0 };
        std::atomic<uint64_t> m_stealAttempts{ 0 };
        std::atomic<uint64_t> m_successfulSteals{ 0 };
        std::atomic<uint64_t> m_sleepCount{ 0 };
        std::atomic<uint64_t> m_queueWaitNs{ 0 };
        std::atomic<uint64_t> m_executionNs{ 0 };
        std::atomic<uint32_t> m_activeWorkers{ 0 };
        std::atomic<uint32_t> m_sleepingWorkers{ 0 };
    };

    JobSystem::JobSystem() : m_impl(std::make_unique<Impl>()) {}

    JobSystem::~JobSystem()
    {
        Shutdown();
    }

    bool JobSystem::Initialize(const JobSystemCreateInfo& createInfo)
    {
        return m_impl->Initialize(createInfo);
    }

    void JobSystem::Shutdown(JobShutdownPolicy policy)
    {
        m_impl->Shutdown(policy);
    }

    void JobSystem::Shutdown()
    {
        m_impl->Shutdown(m_impl->GetCreateInfo().shutdownPolicy);
    }

    JobHandle JobSystem::Schedule(JobDesc desc)
    {
        return m_impl->Schedule(std::move(desc));
    }

    JobHandle JobSystem::Schedule(std::string_view name, SmallJobFunction function, JobTarget target)
    {
        JobDesc desc;
        desc.function = std::move(function);
        desc.nameId = InternName(name);
        desc.target = target;
        return Schedule(std::move(desc));
    }

    JobHandle JobSystem::ScheduleAfter(std::span<const JobHandle> dependencies, JobDesc desc)
    {
        return m_impl->ScheduleAfter(dependencies, std::move(desc));
    }

    JobHandle JobSystem::ScheduleChild(JobHandle parent, JobDesc desc)
    {
        return m_impl->ScheduleChild(parent, std::move(desc));
    }

    void JobSystem::Wait(JobHandle handle)
    {
        m_impl->Wait(handle);
    }

    bool JobSystem::IsComplete(JobHandle handle) const
    {
        return m_impl->IsComplete(handle);
    }

    JobState JobSystem::GetState(JobHandle handle) const
    {
        return m_impl->GetState(handle);
    }

    bool JobSystem::Release(JobHandle handle)
    {
        return m_impl->Release(handle);
    }

    bool JobSystem::Detach(JobHandle handle)
    {
        return m_impl->Detach(handle);
    }

    size_t JobSystem::PumpMainThreadJobs(size_t maximumJobs)
    {
        return m_impl->PumpMainThreadJobs(maximumJobs);
    }

    bool JobSystem::IsInitialized() const
    {
        return m_impl->IsInitialized();
    }

    bool JobSystem::IsAcceptingJobs() const
    {
        return m_impl->IsAcceptingJobs();
    }

    uint32_t JobSystem::GetWorkerCount() const
    {
        return m_impl->GetWorkerCount();
    }

    uint32_t JobSystem::GetParallelForMinItems() const
    {
        return m_impl->GetCreateInfo().parallelForMinItems;
    }

    uint32_t JobSystem::GetMaximumParallelChunks() const
    {
        return m_impl->GetCreateInfo().maximumParallelChunks;
    }

    JobSystemStatistics JobSystem::GetStatistics() const
    {
        return m_impl->GetStatistics();
    }

    uint32_t JobSystem::ResolveWorkerCount(uint32_t requestedWorkers, uint32_t reservedThreads)
    {
        if (requestedWorkers > 0)
            return requestedWorkers;
        const uint32_t hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
        return std::max(1u, hardwareThreads > reservedThreads ? hardwareThreads - reservedThreads : 1u);
    }

    uint32_t JobSystem::InternName(std::string_view name)
    {
        return Profiler::ProfilerNameRegistry::Instance().Intern(name.empty() ? "Job" : name);
    }
} // namespace ChikaEngine::Jobs
