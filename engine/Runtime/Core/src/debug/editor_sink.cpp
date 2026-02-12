
#include "ChikaEngine/debug/editor_sink.h"
#include <mutex>
#include <vector>

namespace ChikaEngine::Debug
{
    void EditorLogSink::Write(LogLevel level, const std::string& msg)
    {
        std::lock_guard lock(_mutex);
        _messagesBuffer.emplace_back(MessagePair(level, msg));
    }

    std::vector<MessagePair> EditorLogSink::FetchMessages()
    {
        std::lock_guard lock(_mutex);
        std::vector<MessagePair> msgs;
        msgs.swap(_messagesBuffer);
        return msgs;
    }
} // namespace ChikaEngine::Debug