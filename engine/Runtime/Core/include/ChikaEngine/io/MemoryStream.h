/*!
 * @file MemoryStream.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief  内存流实现 —— runtime
 * @version 0.1
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/io/IStream.h"
#include <cstdint>
#include <vector>
namespace ChikaEngine::IO
{

    class MemoryStream : public IStream
    {
      public:
        // 初始化不传入数据,只能是写入
        MemoryStream();
        // 初始化传入数据,说明只能是读取
        MemoryStream(const void* data, size_t size);
        ~MemoryStream() = default;

        void Read(void* data, size_t size) override;
        void Write(const void* data, size_t size) override;
        bool IsReadingMode() const override;
        std::size_t GetLength() const override;

      private:
        std::vector<uint8_t> _buffer;
        size_t _pos;
        bool _isReadingMode;
    };
} // namespace ChikaEngine::IO