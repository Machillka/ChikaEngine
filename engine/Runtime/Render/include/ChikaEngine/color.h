#pragma once
namespace ChikaEngine::Render
{
    struct Color
    {
        float r, g, b, a;
        Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
    };
} // namespace ChikaEngine::Render