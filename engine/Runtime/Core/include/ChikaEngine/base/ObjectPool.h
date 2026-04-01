/*!
 * @file ObjectPool.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief  轻量对象池实现
 * @version 0.1
 * @date 2026-03-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <stack>
#include <vector>
namespace ChikaEngine::Core
{
    template <typename T> class ObjectPool
    {
      public:
        ObjectPool(uint32_t initializedSize = 64) {}

        // get 的结果是默认构造
        T* Get()
        {
            if (freeIndices.empty())
            {
                // 扩容一倍
                expend(m_pool.size());
            }

            size_t idx = freeIndices.top();
            freeIndices.pop();

            T* obj = &m_pool[idx];
            *obj = T(); // 默认构造重置
            return obj;
        }
        void Release(T* obj)
        {
            // 指针差, offset 就是下标 pos
            size_t index = obj - m_pool.data();
            *obj = T(); // 默认构造
            freeIndices.push(index);
        }

      private:
        // 扩容
        void expend(size_t count)
        {
            size_t oldSize = m_pool.size();
            m_pool.resize(oldSize + count);

            for (size_t i = oldSize; i < m_pool.size(); ++i)
            {
                freeIndices.push(i);
            }
        }

      private:
        std::vector<T> m_pool;
        std::stack<size_t> freeIndices; // 存放空闲下标 FISO
    };
} // namespace ChikaEngine::Core