#include "ChikaEngine/debug/log_sink.h"
#include "ChikaEngine/debug/console_sink.h"

#include <iostream>
#include <ostream>
#include <string>

namespace ChikaEngine::Debug
{
    void ConsoleLogSink::Write(LogLevel level, const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

} // namespace ChikaEngine::Debug