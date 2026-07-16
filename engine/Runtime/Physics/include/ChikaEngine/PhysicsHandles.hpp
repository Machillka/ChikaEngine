#pragma once

#include <cstddef>
#include <cstdint>

namespace ChikaEngine::Physics
{
    /**
     * @brief Backend-independent physics handle with stale-reference protection.
     *
     * The low 32 bits identify a registry slot and the high 32 bits identify
     * the generation currently occupying that slot. A zero value is always
     * invalid. Backend-native identifiers must never cross this boundary.
     */
    template <typename Tag> class PhysicsHandle
    {
      public:
        using ValueType = std::uint64_t;

        constexpr PhysicsHandle() noexcept = default;

        [[nodiscard]] static constexpr PhysicsHandle Invalid() noexcept
        {
            return {};
        }

        [[nodiscard]] static constexpr PhysicsHandle FromParts(std::uint32_t index, std::uint32_t generation) noexcept
        {
            if (generation == 0)
                return Invalid();
            return PhysicsHandle((static_cast<ValueType>(generation) << 32u) | static_cast<ValueType>(index));
        }

        [[nodiscard]] static constexpr PhysicsHandle FromValue(ValueType value) noexcept
        {
            if ((value >> 32u) == 0)
                return Invalid();
            return PhysicsHandle(value);
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return _value != 0;
        }

        [[nodiscard]] constexpr std::uint32_t Index() const noexcept
        {
            return static_cast<std::uint32_t>(_value & 0xFFFFFFFFull);
        }

        [[nodiscard]] constexpr std::uint32_t Generation() const noexcept
        {
            return static_cast<std::uint32_t>(_value >> 32u);
        }

        [[nodiscard]] constexpr ValueType Value() const noexcept
        {
            return _value;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        [[nodiscard]] constexpr bool operator==(const PhysicsHandle& other) const noexcept
        {
            return _value == other._value;
        }

        [[nodiscard]] constexpr bool operator!=(const PhysicsHandle& other) const noexcept
        {
            return !(*this == other);
        }

      private:
        explicit constexpr PhysicsHandle(ValueType value) noexcept : _value(value) {}

        ValueType _value = 0;
    };

    struct PhysicsBodyHandleTag;
    struct PhysicsColliderHandleTag;

    using PhysicsBodyHandle = PhysicsHandle<PhysicsBodyHandleTag>;
    using PhysicsColliderHandle = PhysicsHandle<PhysicsColliderHandleTag>;

    struct PhysicsHandleHash
    {
        template <typename Tag> std::size_t operator()(PhysicsHandle<Tag> handle) const noexcept
        {
            // SplitMix64 finalizer keeps index and generation entropy without
            // pulling the large <functional> header into reflection inputs.
            std::uint64_t value = handle.Value();
            value ^= value >> 30u;
            value *= 0xBF58476D1CE4E5B9ull;
            value ^= value >> 27u;
            value *= 0x94D049BB133111EBull;
            value ^= value >> 31u;
            return static_cast<std::size_t>(value);
        }
    };
} // namespace ChikaEngine::Physics
