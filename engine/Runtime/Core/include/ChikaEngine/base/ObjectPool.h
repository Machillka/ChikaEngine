#pragma once

#include <concepts>
#include <cstddef>
#include <deque>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ChikaEngine::Core
{
    /**
     * @brief A single-threaded object pool with stable addresses across growth.
     *
     * Acquired objects remain valid until Release() or Clear(). The pool owns every
     * object and destroys active objects when the pool itself is destroyed. Callers
     * must not retain or use a pointer after releasing it.
     */
    template <typename T> class ObjectPool
    {
      public:
        explicit ObjectPool(size_t initialCapacity = 64)
        {
            Grow(initialCapacity);
        }

        ObjectPool(const ObjectPool&) = delete;
        ObjectPool& operator=(const ObjectPool&) = delete;
        ObjectPool(ObjectPool&&) = delete;
        ObjectPool& operator=(ObjectPool&&) = delete;

        /** @brief Acquires a default-constructed object. */
        [[nodiscard]] T* Get()
            requires std::default_initializable<T>
        {
            return Emplace();
        }

        /** @brief Acquires an object constructed from the supplied arguments. */
        template <typename... Args>
            requires std::constructible_from<T, Args...>
        [[nodiscard]] T* Emplace(Args&&... args)
        {
            if (m_freeIndices.empty())
                Grow(m_slots.empty() ? 1 : m_slots.size());

            const size_t index = m_freeIndices.back();
            m_freeIndices.pop_back();
            Slot& slot = m_slots[index];

            try
            {
                T* object = &slot.value.emplace(std::forward<Args>(args)...);
                m_activeIndices.emplace(object, index);
                return object;
            }
            catch (...)
            {
                slot.value.reset();
                m_freeIndices.push_back(index);
                throw;
            }
        }

        /**
         * @brief Releases an acquired object.
         * @return false for null, foreign, or already released pointers.
         */
        bool Release(T* object)
        {
            const auto active = m_activeIndices.find(object);
            if (active == m_activeIndices.end())
                return false;

            const size_t index = active->second;
            m_slots[index].value.reset();
            m_activeIndices.erase(active);
            m_freeIndices.push_back(index);
            return true;
        }

        /** @brief Destroys all active objects while retaining allocated capacity. */
        void Clear()
        {
            for (Slot& slot : m_slots)
                slot.value.reset();

            m_activeIndices.clear();
            m_freeIndices.clear();
            m_freeIndices.reserve(m_slots.size());
            for (size_t index = 0; index < m_slots.size(); ++index)
                m_freeIndices.push_back(index);
        }

        [[nodiscard]] bool IsAcquired(const T* object) const
        {
            return m_activeIndices.contains(const_cast<T*>(object));
        }

        [[nodiscard]] size_t Capacity() const
        {
            return m_slots.size();
        }

        [[nodiscard]] size_t InUseCount() const
        {
            return m_activeIndices.size();
        }

        [[nodiscard]] size_t AvailableCount() const
        {
            return m_freeIndices.size();
        }

      private:
        struct Slot
        {
            std::optional<T> value;
        };

        void Grow(size_t count)
        {
            if (count == 0)
                return;
            if (count > m_slots.max_size() - m_slots.size())
                throw std::length_error("ObjectPool capacity overflow");

            const size_t oldSize = m_slots.size();
            const size_t newSize = oldSize + count;
            m_freeIndices.reserve(newSize);
            m_activeIndices.reserve(newSize);
            m_slots.resize(newSize);
            for (size_t index = oldSize; index < newSize; ++index)
                m_freeIndices.push_back(index);
        }

        // deque growth preserves addresses of existing slots and their live objects.
        std::deque<Slot> m_slots;
        std::vector<size_t> m_freeIndices;
        std::unordered_map<T*, size_t> m_activeIndices;
    };
} // namespace ChikaEngine::Core
