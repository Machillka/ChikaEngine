#pragma once

#include "framework/component/Component.h"
#include "framework/component/Renderable.h"
#include "framework/gameobject/GameObject.h"
#include "render/renderobject.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
namespace ChikaEngine::Framework
{
    // 可被管理的 Scene 实例
    // TODO: 实现 Scene Manager 管理多个 Scene 支持切换加载等
    // TODO: 序列化实现 json 读写关卡
    class Scene
    {
      public:
        static Scene& Instance();
        GameObject* CreateGO(const std::string& name = "");
        void DestroyGO(GameObjectID id);
        void DestoryGO(GameObject* go);
        void Update(float deltaTime);
        void FixedUpdate(float fixedDeltaTime);
        GameObject* GetGOByID(GameObjectID id);
        std::vector<GameObject> GetAllGameObjects();
        std::vector<Render::RenderObject> GetAllVisiableRenderObjects();

        void RegisterRenderable(Renderable* comp);
        void UnregisterRenderable(Renderable* comp);

      private:
        Scene() = default;
        ~Scene() = default;
        // 缓存并且管理 scene 下的所有 GO
        std::unordered_map<GameObjectID, std::unique_ptr<GameObject>> _objects;

        std::mutex _renderMutex;
        std::vector<Renderable*> _renderables;
    };
} // namespace ChikaEngine::Framework