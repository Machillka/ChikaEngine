#pragma once

#include "log_sink.h"

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ChikaEngine::Debug
{
    typedef std::pair<LogLevel, std::string> MessagePair;
    class EditorLogSink : public ILogSink
    {
      public:
        void Write(LogLevel level, const std::string& msg) override;
        std::vector<MessagePair> FetchMessages();

      private:
        std::mutex _mutex;
        std::vector<MessagePair> _messagesBuffer;
    };
} // namespace ChikaEngine::Debug