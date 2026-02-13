#include "ChikaEngine/io/MemoryStream.h"
namespace ChikaEngine::IO
{
    MemoryStream::MemoryStream() : _isReadingMode(false), _pos(0) {}

    MemoryStream::MemoryStream(const void* data, size_t size) : _isReadingMode(true), _pos(0)
    {
        _buffer.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    }

    void MemoryStream::Read(void* data, size_t size)
    {
        if (_pos + size > _buffer.size())
            return;
        std::memcpy(data, _buffer.data() + _pos, size);
        _pos += size;
    }
    void MemoryStream::Write(const void* data, size_t size)
    {
        if (_isReadingMode)
            return;
        if (_pos + size > _buffer.size())
            _buffer.resize(_pos + size);
        std::memcpy(_buffer.data() + _pos, data, size);
        _pos += size;
    }
    bool MemoryStream::IsReadingMode() const
    {
        return _isReadingMode;
    }
    size_t MemoryStream::GetLength() const
    {
        return _buffer.size();
    }
} // namespace ChikaEngine::IO