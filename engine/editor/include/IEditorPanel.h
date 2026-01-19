#pragma once

namespace ChikaEngine::Editor
{
    struct UIContext;
    class IEditorPanel
    {
        virtual ~IEditorPanel() = default;
        // 返回名称
        virtual const char* Name() const = 0;
        virtual void OnRender(UIContext &ctx) = 0;
    };
} // namespace ChikaEngine::Editor