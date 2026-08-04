#pragma once

#include "ChikaEngine/PhysicsDescs.h"

#include <cstdint>

namespace ChikaEngine::Physics
{
    /**
     * @brief Owns process-wide physics backend registration through leases.
     *
     * A PhysicsScene owns a Lease through its backend. The first lease performs
     * global Jolt registration and the final lease performs global shutdown,
     * allowing multiple independent physics worlds to coexist safely.
     */
    class PhysicsRuntime final
    {
      public:
        struct Statistics
        {
            std::uint32_t activeLeases = 0;
            std::uint64_t initializationCount = 0;
            std::uint64_t shutdownCount = 0;
            bool initialized = false;
        };

        class Lease final
        {
          public:
            Lease() = default;
            ~Lease();

            Lease(const Lease&) = delete;
            Lease& operator=(const Lease&) = delete;
            Lease(Lease&& other) noexcept;
            Lease& operator=(Lease&& other) noexcept;

            [[nodiscard]] bool IsActive() const noexcept
            {
                return _active;
            }

            void Reset() noexcept;

          private:
            friend class PhysicsRuntime;
            bool _active = false;
        };

        [[nodiscard]] static PhysicsResult Acquire(Lease& lease);
        [[nodiscard]] static Statistics GetStatistics();

      private:
        static void Release() noexcept;
    };
} // namespace ChikaEngine::Physics
