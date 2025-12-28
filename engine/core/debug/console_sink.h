#include "debug/log_sink.h"

#include <string>

namespace ChikaEngine::Debug
{
    class ConsoleLogSink : public ILogSink
    {
      public:
        void Write(LogLevel level, const std::string& msg) override;
    };
} // namespace ChikaEngine::Debug