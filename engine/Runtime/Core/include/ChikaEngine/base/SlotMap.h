#pragma once

#include <cstdint>
#include <vector>
namespace ChikaEngine::Core
{

    template <typename Handle, typename T> class SlotMap
    {
      public:
        SlotMap() = default;

        Handle Create(const T& value)
        {
            m_aliveCount++;

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
            e.value = value;
            e.alive = true;

            Handle h = Handle::FromParts(index, e.generation);

            return h;
        }

        void Destroy(Handle h)
        {
            m_aliveCount--;

            uint32_t index = h.GetIndex();
            if (index >= m_entries.size())
                return;

            Entry& e = m_entries[index];
            if (!e.alive)
                return;

            // 标记死亡
            e.alive = false;

            // generation++
            e.generation++;

            // 放入 free list
            m_freeList.push_back(index);
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

        std::vector<Entry> m_entries;
        std::vector<uint32_t> m_freeList;

        uint32_t m_aliveCount = 0;
    };
} // namespace ChikaEngine::Core