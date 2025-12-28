
#include "debug/file_sink.h"

namespace ChikaEngine::Debug
{
    FileLogSink::FileLogSink(const std::string& filepath) : _file(filepath, std::ios::app) {}
    void FileLogSink::Write(LogLevel level, const std::string &msg)
    {
        if (!_file.is_open())
            return;
        _file << msg << std::endl;
        _file.flush();
    }
} // namespace ChikaEngine::Debug