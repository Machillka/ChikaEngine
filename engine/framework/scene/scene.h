#pragma once

#include "framework/gameobject/GameObject.h"
#include "render/renderobject.h"

#include <memory>
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
        void Tick(float dt);
        GameObject* GetGOByID(GameObjectID id);
        std::vector<GameObject> GetAllGameObjects();
        std::vector<Render::RenderObject> GetAllVisiableRenderObjects();

      private:
        Scene();
        ~Scene();
        // 缓存并且管理 scene 下的所有 GO
        std::unordered_map<GameObjectID, std::unique_ptr<GameObject>> _objects;
    };
} // namespace ChikaEngine::Framework