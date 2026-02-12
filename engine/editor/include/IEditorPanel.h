#pragma once

#include <string>
namespace ChikaEngine::Editor
{
    struct SelectionContext
    {
        void* objectPtr = nullptr; // 指向选中对象的指针
        std::string fullName;      // 完整类名
    };
    struct UIContext
    {
        float deltaTime;
        SelectionContext selection;
    };
    class IEditorPanel
    {
      public:
        virtual ~IEditorPanel() = default;
        // 返回名称
        virtual const char* Name() const = 0;
        virtual void OnRender(UIContext& ctx) = 0;
    };
} // namespace ChikaEngine::Editor