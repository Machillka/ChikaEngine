#pragma once
#include "ChikaEngine/debug/log_sink.h"

#include <fstream>
#include <string>

namespace ChikaEngine::Debug
{

    class FileLogSink : public ILogSink
    {
      public:
        explicit FileLogSink(const std::string& filepath);
        void Write(LogLevel level, const std::string& msg) override;

      private:
        std::ofstream _file;
    };
} // namespace ChikaEngine::Debug