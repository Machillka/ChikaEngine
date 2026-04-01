#pragma once
namespace ChikaEngine::Render
{
    // 对不同后端实现的索引类型进行抽象
    enum class IndexType
    {
        Uint16,
        Uint32
    };
} // namespace ChikaEngine::Render