#pragma once
#include "framework/gameobject/GameObject.h"
#include "render/camera.h"
#include "render/renderobject.h"
#include "resource_importer/include/ResourceSystem.h"

namespace ChikaEngine::Platform
{
    class IWindow;
}

#include <vector>

namespace ChikaEngine::Engine
{
    class GameEngine
    {
      public:
        GameEngine();
        ~GameEngine();
        // 初始化各个系统
        void Initialize(ChikaEngine::Platform::IWindow* window);
        void Tick();
        void Render();
        void Shutdown();
        std::vector<Render::RenderObject>* RenderObjects();

      private:
        Platform::IWindow* _window;
        Render::Camera _camera;
        ChikaEngine::Resource::ResourceSystem _resourceSystem;
        Framework::GameObject* goTest;
        // TODO: 加入 level 等 实现 GamePlay
    };
} // namespace ChikaEngine::Engine
