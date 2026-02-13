/*!
 * @file FileStream.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief 封装文件流
 * @version 0.1
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/io/IStream.h"
#include <fstream>
#include <string>
namespace ChikaEngine::IO
{

    class FileStream : public IStream
    {
      public:
        FileStream(const std::string& path, Mode mode);
        ~FileStream();

        void Read(void* data, size_t size) override;
        void Write(const void* data, size_t size) override;
        bool IsReadingMode() const override;
        size_t GetLength() const override;

      private:
        std::fstream _stream;
        bool _isReadingMode;
    };
} // namespace ChikaEngine::IO