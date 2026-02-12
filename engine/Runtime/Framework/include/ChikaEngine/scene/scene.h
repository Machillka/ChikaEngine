#pragma once

#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/gameobject/camera.h"
#include "ChikaEngine/renderobject.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
namespace ChikaEngine::Framework
{
    // 可被管理的 Scene 实例
    // TODO[x]: 实现 Scene Manager 管理多个 Scene 支持切换加载等
    // TODO: 序列化实现 json 读写关卡
    class Scene
    {
      public:
        Scene();
        ~Scene();

        GameObject* CreateGO(const std::string& name = "");
        void DestroyGO(GameObjectID id);
        void DestoryGO(GameObject* go);

        void Update(float deltaTime);
        void FixedUpdate(float fixedDeltaTime);
        GameObject* GetGOByID(GameObjectID id);
        std::vector<GameObject> GetAllGameObjects();
        std::vector<Render::RenderObject> GetAllVisiableRenderObjects();

        void OnRuntimeStart();

        // 游戏结束：清理物理，销毁运行时数据
        void OnRuntimeStop();

        // 运行时更新：处理物理模拟、脚本 Update
        void OnUpdateRuntime(float dt);

        // 编辑器更新：只处理编辑器摄像机、Gizmos，不跑物理, 把 Camera 移动到 Scene 管理,而不是 Editor
        void OnUpdateEditor(float dt, Camera* editorCamera);
        void OnViewportResize(uint32_t width, uint32_t height);
        void RegisterRenderable(Renderable* comp);
        void UnregisterRenderable(Renderable* comp);

      private:
        // 缓存并且管理 scene 下的所有 GO
        std::unordered_map<GameObjectID, std::unique_ptr<GameObject>> _objects;

        std::mutex _renderMutex;
        std::vector<Renderable*> _renderables;
        bool _isRunning = false;
    };
} // namespace ChikaEngine::Framework