/*!
 * @file BinaryLoadArchive.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief  Load -> 从 IO 流中加载东西 ( 还原 )
 * @version 0.1
 * @date 2026-02-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/io/IStream.h"
#include "ChikaEngine/serialization/Access.h"
#include "ChikaEngine/debug/log_macros.h"
#include <cstddef>
#include <string>
namespace ChikaEngine::Serialization
{
    class BinaryLoadArchive
    {
      public:
        static constexpr bool IsSaving = false;
        static constexpr bool IsLoading = true;

        BinaryLoadArchive(IO::IStream& stream) : _stream(stream) {}

        template <typename T> void operator()(const NVP<T>& nvp)
        {
            Serializer::Dispatch(*this, nullptr, nvp.Value);
        }

        template <typename T> void operator()(const T& val)
        {
            Serializer::Dispatch(*this, nullptr, const_cast<T&>(val));
        }

        template <typename T> void operator()(const char* name, T& val)
        {
            Serializer::Dispatch(*this, nullptr, val);
        }

        void EnterNode(const char* name) {}
        void LeaveNode() {}

        void EnterArray(const char* name, size_t& size)
        {
            _stream.Read(&size, sizeof(size_t));
        }
        void LeaveArray() {}

        template <typename T> void ProcessValue(const char* name, T& val)
        {
            if constexpr (std::is_same_v<T, std::string>)
            {
                size_t len = 0;
                _stream.Read(&len, sizeof(size_t));
                val.resize(len);
                if (len > 1024 * 1024 * 1000)
                {
                    LOG_ERROR("Serialization", "String length too large: {}. Stream corruption suspected.", len);
                    val = ""; // 设为空防止崩溃
                    return;
                }
                if (len > 0)
                    _stream.Read(val.data(), len);
            }
            else if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>)
            {
                _stream.Read(&val, sizeof(T));
            }
            else
            {
                val.serialize(*this);
            }
        }

      private:
        IO::IStream& _stream;
    };

} // namespace ChikaEngine::Serialization