#pragma once
#include "IEditorPanel.h"

#include <string>

namespace ChikaEngine::Editor
{
    class IEditorUI
    {
      public:
        virtual ~IEditorUI() = default;
        virtual bool Init(void* nativeWindow) = 0;
        virtual void NewFrame() = 0;
        virtual void RegisterPanel(IEditorPanel* panel) = 0;
        virtual void UnregisterPanel(IEditorPanel* panel) = 0;
        virtual void RenderAllPanels() = 0; // 构建UI
        virtual void Render() = 0;          // 真的渲染
        virtual void SwapBuffers() = 0;
        virtual void Shutdown() = 0;
        virtual void SaveLayout(const std::string& path) = 0;
        virtual void LoadLayout(const std::string& path) = 0;
        virtual void UpdateContext() = 0;
    };
} // namespace ChikaEngine::Editor