#pragma once

#include "ChikaEngine/Renderer.hpp"
#include "EditorContext.hpp"
#include "IEditorPanel.hpp"
#include <memory>
#include <utility>
#include <vector>

class GLFWwindow;

namespace ChikaEngine::Editor
{

    struct EditorCreateInfo
    {
        Render::Renderer* renderer;
        void* window; // TODO: 重构窗口类
    };

    class EditorManager
    {
      public:
        void Initialize(const EditorCreateInfo& createInfo);
        void Shutdown();
        void Tick(float deltaTime);
        void OnImGuiRender();

        void BeginFrame();
        void EndFrame();

        template <typename T, typename... Args> void AddPanel(Args&&... args)
        {
            auto panel = std::make_unique<T>(std::forward(args)...);
            panel->Initialize(&_context);
            _panels.push_back(std::move(panel));
        }

      private:
        void BeginDockspace();
        void EndDockspace();
        void ApplyDefaultLayout();

      private:
        EditorContext _context;
        std::vector<std::unique_ptr<IEditorPanel>> _panels;
        GLFWwindow* _windowHandle;
        Render::Renderer* _renderer;
    };
} // namespace ChikaEngine::Editor