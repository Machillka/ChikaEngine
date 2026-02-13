#include "ChikaEngine/io/FileStream.h"
namespace ChikaEngine::IO
{

    FileStream::FileStream(const std::string& path, Mode mode)
    {
        _isReadingMode = (mode == Mode::Read);
        auto flags = std::ios::binary | (_isReadingMode ? std::ios::in : std::ios::out);
        _stream.open(path, flags);
        if (!_stream.is_open())
            throw std::runtime_error("Open file failed: " + path);
    }
    FileStream::~FileStream()
    {
        if (_stream.is_open())
            _stream.close();
    }

    void FileStream::Read(void* data, size_t size)
    {
        _stream.read((char*)data, size);
    }
    void FileStream::Write(const void* data, size_t size)
    {
        _stream.write((const char*)data, size);
    }
    bool FileStream::IsReadingMode() const
    {
        return _isReadingMode;
    }
    size_t FileStream::GetLength() const
    {
        // 强制转换以使用非 const 的 seek/tell，或者使用 mutable
        auto& s = const_cast<std::fstream&>(_stream);

        size_t currentPos = _isReadingMode ? s.tellg() : s.tellp();

        s.seekg(0, std::ios::end);
        size_t length = s.tellg();

        // 恢复指针位置
        s.seekg(currentPos, std::ios::beg);

        return length;
    }

} // namespace ChikaEngine::IO