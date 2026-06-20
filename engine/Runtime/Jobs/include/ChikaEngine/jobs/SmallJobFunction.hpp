#pragma once

#include <concepts>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace ChikaEngine::Jobs
{
    /** @brief Move-only callable with fixed inline storage for allocation-free regular jobs. */
    class SmallJobFunction
    {
      public:
        static constexpr size_t InlineSize = 128;

        SmallJobFunction() = default;

        template <typename Function>
            requires(!std::same_as<std::remove_cvref_t<Function>, SmallJobFunction>)
        explicit SmallJobFunction(Function&& function)
        {
            using Stored = std::remove_cvref_t<Function>;
            static_assert(sizeof(Stored) <= InlineSize, "Job callable exceeds SmallJobFunction inline capacity");
            static_assert(alignof(Stored) <= alignof(std::max_align_t), "Job callable requires unsupported alignment");
            static_assert(std::is_nothrow_move_constructible_v<Stored>, "Job callable must be nothrow move constructible");
            new (m_storage) Stored(std::forward<Function>(function));
            m_invoke = [](void* storage) { (*static_cast<Stored*>(storage))(); };
            m_move = [](void* source, void* destination)
            {
                new (destination) Stored(std::move(*static_cast<Stored*>(source)));
                static_cast<Stored*>(source)->~Stored();
            };
            m_destroy = [](void* storage) { static_cast<Stored*>(storage)->~Stored(); };
        }

        SmallJobFunction(SmallJobFunction&& other) noexcept
        {
            MoveFrom(std::move(other));
        }

        SmallJobFunction& operator=(SmallJobFunction&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                MoveFrom(std::move(other));
            }
            return *this;
        }

        ~SmallJobFunction()
        {
            Reset();
        }

        SmallJobFunction(const SmallJobFunction&) = delete;
        SmallJobFunction& operator=(const SmallJobFunction&) = delete;

        explicit operator bool() const
        {
            return m_invoke != nullptr;
        }

        void operator()()
        {
            m_invoke(m_storage);
        }

        void Reset()
        {
            if (m_destroy)
                m_destroy(m_storage);
            m_invoke = nullptr;
            m_move = nullptr;
            m_destroy = nullptr;
        }

      private:
        void MoveFrom(SmallJobFunction&& other) noexcept
        {
            m_invoke = other.m_invoke;
            m_move = other.m_move;
            m_destroy = other.m_destroy;
            if (m_move)
                m_move(other.m_storage, m_storage);
            other.m_invoke = nullptr;
            other.m_move = nullptr;
            other.m_destroy = nullptr;
        }

        alignas(std::max_align_t) std::byte m_storage[InlineSize]{};
        void (*m_invoke)(void*) = nullptr;
        void (*m_move)(void*, void*) = nullptr;
        void (*m_destroy)(void*) = nullptr;
    };
} // namespace ChikaEngine::Jobs
