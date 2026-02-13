#pragma once

#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/gameobject/Camera.h"
#include "ChikaEngine/renderobject.h"
#include "ChikaEngine/scene/scene.h"
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
        Framework::Scene* _scene = nullptr;

      private:
        Platform::IWindow* _window = nullptr;
        Framework::GameObject* goTest = nullptr;

        // TODO: 加入 level 等 实现 GamePlay
    };
} // namespace ChikaEngine::Engine
