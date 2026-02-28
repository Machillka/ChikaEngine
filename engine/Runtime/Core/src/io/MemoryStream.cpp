#include "ChikaEngine/io/MemoryStream.h"
#include "ChikaEngine/debug/log_macros.h"
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
        {
            // 关键：读取失败时清零输出，防止调用方使用随机垃圾值
            std::memset(data, 0, size);
            LOG_ERROR("IO", "MemoryStream Read out of bounds! Pos: {}, Size: {}, Buffer: {}", _pos, size, _buffer.size());
            return;
        }
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

    // 直接反转到读取数据,重置游标到 0, 直接读曾经存在里面的数据
    void MemoryStream::FlipToRead()
    {
        _isReadingMode = true;
        _pos = 0;
    }
    size_t MemoryStream::GetLength() const
    {
        return _buffer.size();
    }
    const std::vector<uint8_t>& MemoryStream::GetRawData()
    {
        return _buffer;
    }
} // namespace ChikaEngine::IO