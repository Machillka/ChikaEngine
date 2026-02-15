/*!
 * @file BinarySaveArchive.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief  Save -> 把数据保存到 IO 流中
 * @version 0.1
 * @date 2026-02-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/io/IStream.h"
#include "ChikaEngine/serialization/Access.h"
#include <cstddef>
#include <string>
namespace ChikaEngine::Serialization
{
    class BinarySaveArchive
    {
      public:
        static constexpr bool IsSaving = true;
        static constexpr bool IsLoading = false;

        BinarySaveArchive(IO::IStream& stream) : _stream(stream)
        {
            LOG_INFO("io", "Stream received, {}", stream.IsReadingMode());
        }

        template <typename T> void operator()(const NVP<T>& nvp)
        {
            Serializer::Dispatch(*this, nullptr, nvp.value);
        }

        template <typename T> void operator()(const T& val)
        {
            Serializer::Dispatch(*this, nullptr, const_cast<T&>(val));
        }

        template <typename T> void operator()(const char* name, const T& val)
        {
            Serializer::Dispatch(*this, nullptr, val);
        }

        // --- Duck Interface ---
        void EnterNode(const char* name) {} // 二进制不需要 Scope
        void LeaveNode() {}

        void EnterArray(const char* name, size_t size)
        {
            _stream.Write(&size, sizeof(size_t)); // 写入长度
        }
        void LeaveArray() {}

        template <typename T> void ProcessValue(const char* name, const T& val)
        {
            LOG_WARN("Archive", "enter");
            // 字符串特殊处理
            if constexpr (std::is_same_v<T, std::string>)
            {
                size_t len = val.size();
                _stream.Write(&len, sizeof(size_t));
                if (len > 0)
                    _stream.Write(val.data(), len);
            }
            else
            {
                _stream.Write(&val, sizeof(T));
            }
        }

      private:
        IO::IStream& _stream;
    };

} // namespace ChikaEngine::Serialization