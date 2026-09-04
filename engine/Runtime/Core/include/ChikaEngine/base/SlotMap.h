#pragma once

#include <cstdint>
#include <deque>
#include <ranges>
#include <utility>
#include <vector>
namespace ChikaEngine::Core
{

    template <typename Handle, typename T> class SlotMap
    {
      public:
        SlotMap() = default;

        Handle Create(const T& value)
        {
            uint32_t index;

            if (!m_freeList.empty())
            {
                index = m_freeList.back();
                m_freeList.pop_back();
            }
            else
            {
                index = static_cast<uint32_t>(m_entries.size());
                m_entries.push_back({});
            }

            Entry& e = m_entries[index];
            e.value = value; // 假设此处不会抛出异常
            e.alive = true;

            Handle h = Handle::FromParts(index, e.generation);

            m_aliveCount++;

            return h;
        }

        // 对于右值引用的传入, 使用移动进行构造
        Handle Create(T&& value)
        {
            uint32_t index;

            if (!m_freeList.empty())
            {
                index = m_freeList.back();
                Entry& e = m_entries[index];
                e.value = std::move(value);
                e.alive = true;
                m_freeList.pop_back();
            }
            else
            {
                index = static_cast<uint32_t>(m_entries.size());
                m_entries.emplace_back(Entry{ .value = std::move(value), .alive = true });
            }

            Entry& e = m_entries[index];

            Handle h = Handle::FromParts(index, e.generation);

            m_aliveCount++;

            return h;
        }

        void Destroy(Handle h)
        {
            uint32_t index = h.GetIndex();
            if (index >= m_entries.size())
                return;

            Entry& e = m_entries[index];
            if (!e.alive || e.generation != h.GetGen())
                return;

            // 放入 free list
            m_freeList.push_back(index);

            m_aliveCount--;

            // 标记死亡
            e.alive = false;

            // generation++
            e.generation++;
        }

        T* Get(Handle h)
        {
            uint32_t index = h.GetIndex();
            if (index >= m_entries.size())
                return nullptr;

            Entry& e = m_entries[index];

            // generation 不匹配 → 旧 handle
            if (!e.alive || e.generation != h.GetGen())
                return nullptr;

            return &e.value;
        }

        const T* Get(Handle h) const
        {
            return const_cast<SlotMap*>(this)->Get(h);
        }

        void Clear()
        {
            m_entries.clear();
            m_freeList.clear();
            m_aliveCount = 0;
        }

        template <typename Func> void ForEach(Func&& func)
        {
            for (uint32_t i = 0; i < m_entries.size(); ++i)
            {
                Entry& e = m_entries[i];
                if (!e.alive)
                    continue;

                Handle h = Handle::FromParts(i, e.generation);
                func(h, e.value);
            }
        }

        /**
         * @brief 遍历只读存活槽位，用于生成不可变快照而不暴露可变存储。
         */
        template <typename Func> void ForEach(Func&& func) const
        {
            for (uint32_t i = 0; i < m_entries.size(); ++i)
            {
                const Entry& e = m_entries[i];
                if (!e.alive)
                    continue;

                Handle h = Handle::FromParts(i, e.generation);
                func(h, e.value);
            }
        }

        const uint32_t Size() const
        {
            return m_aliveCount;
        }

      private:
        struct Entry
        {
            T value{};
            uint32_t generation = 1;
            bool alive = false;
        };

        // Deque keeps existing asset pointers stable while new asynchronous loads append slots.
        std::deque<Entry> m_entries;
        std::vector<uint32_t> m_freeList;

        uint32_t m_aliveCount = 0;
    };
} // namespace ChikaEngine::Core
