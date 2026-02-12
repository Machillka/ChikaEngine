#pragma once

#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/gameobject/camera.h"
#include "ChikaEngine/renderobject.h"
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
        Framework::Camera _camera;
        Framework::GameObject* goTest;
        // TODO: 加入 level 等 实现 GamePlay
    };
} // namespace ChikaEngine::Engine
