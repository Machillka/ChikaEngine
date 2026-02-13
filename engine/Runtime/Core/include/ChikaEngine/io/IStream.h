/*!
 * @file IStream.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief  定义数据流抽象
 * @version 0.1
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
namespace ChikaEngine::IO
{
    enum class Mode
    {
        Read,
        Write
    };
    class IStream
    {
      public:
        virtual ~IStream() = default;
        // 所有流 都是读写 字节
        virtual void Read(void* data, std::size_t size) = 0;
        virtual void Write(const void* data, std::size_t size) = 0;
        // 获得字节长度
        virtual size_t GetLength() const = 0;
        virtual bool IsReadingMode() const = 0;
    };
} // namespace ChikaEngine::IO