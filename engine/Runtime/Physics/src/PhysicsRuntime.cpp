#include "ChikaEngine/PhysicsRuntime.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/RegisterTypes.h>

#include <exception>
#include <mutex>
#include <new>
#include <string>

namespace ChikaEngine::Physics
{
    namespace
    {
        struct PhysicsRuntimeState
        {
            std::mutex mutex;
            std::uint32_t activeLeases = 0;
            std::uint64_t initializationCount = 0;
            std::uint64_t shutdownCount = 0;
            bool initialized = false;
        };

        PhysicsRuntimeState& GetRuntimeState()
        {
            static PhysicsRuntimeState state;
            return state;
        }
    } // namespace

    PhysicsRuntime::Lease::~Lease()
    {
        Reset();
    }

    PhysicsRuntime::Lease::Lease(Lease&& other) noexcept : _active(other._active)
    {
        other._active = false;
    }

    PhysicsRuntime::Lease& PhysicsRuntime::Lease::operator=(Lease&& other) noexcept
    {
        if (this == &other)
            return *this;

        Reset();
        _active = other._active;
        other._active = false;
        return *this;
    }

    void PhysicsRuntime::Lease::Reset() noexcept
    {
        if (!_active)
            return;

        _active = false;
        PhysicsRuntime::Release();
    }

    PhysicsResult PhysicsRuntime::Acquire(Lease& lease)
    {
        if (lease._active)
            return PhysicsResult{ .status = PhysicsStatus::AlreadyInitialized, .diagnostic = "Physics runtime lease is already active" };

        auto& state = GetRuntimeState();
        std::lock_guard lock(state.mutex);

        if (state.activeLeases == 0)
        {
            if (JPH::Factory::sInstance != nullptr)
                return PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Jolt Factory is already owned outside PhysicsRuntime");

            try
            {
                JPH::RegisterDefaultAllocator();
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
            }
            catch (const std::exception& exception)
            {
                if (JPH::Factory::sInstance != nullptr)
                {
                    JPH::UnregisterTypes();
                    delete JPH::Factory::sInstance;
                    JPH::Factory::sInstance = nullptr;
                }
                return PhysicsResult::Failure(PhysicsStatus::BackendFailure, std::string("Failed to initialize Jolt runtime: ") + exception.what());
            }
            catch (...)
            {
                if (JPH::Factory::sInstance != nullptr)
                {
                    JPH::UnregisterTypes();
                    delete JPH::Factory::sInstance;
                    JPH::Factory::sInstance = nullptr;
                }
                return PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Failed to initialize Jolt runtime");
            }

            state.initialized = true;
            ++state.initializationCount;
        }

        ++state.activeLeases;
        lease._active = true;
        return PhysicsResult::Ok();
    }

    PhysicsRuntime::Statistics PhysicsRuntime::GetStatistics()
    {
        auto& state = GetRuntimeState();
        std::lock_guard lock(state.mutex);
        return Statistics{
            .activeLeases = state.activeLeases,
            .initializationCount = state.initializationCount,
            .shutdownCount = state.shutdownCount,
            .initialized = state.initialized,
        };
    }

    void PhysicsRuntime::Release() noexcept
    {
        auto& state = GetRuntimeState();
        std::lock_guard lock(state.mutex);

        if (state.activeLeases == 0)
            return;

        --state.activeLeases;
        if (state.activeLeases != 0)
            return;

        if (JPH::Factory::sInstance != nullptr)
        {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }

        state.initialized = false;
        ++state.shutdownCount;
    }
} // namespace ChikaEngine::Physics
