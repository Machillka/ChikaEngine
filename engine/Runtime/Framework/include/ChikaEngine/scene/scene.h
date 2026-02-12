#pragma once

#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/renderobject.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ChikaEngine::Framework
{
    // 可被管理的 Scene 实例
    // TODO[x]: 实现 Scene Manager 管理多个 Scene 支持切换加载等
    // TODO: 序列化实现 json 读写关卡
    /*!
        不论是 play mode 还是 edit mode
        处理的 GO 是完全一致的,包括是否要 进行渲染
        基于此, 不需要改动 go 相关逻辑,只需要记录 edit 时候 go 的各个状态;
        然后在 play mode 启动的时候执行一个快照
        最后在 play mode 销毁的时候, 恢复快照
    */
    class Scene
    {
      public:
        Scene();
        ~Scene();

        GameObject* CreateGameObject(const std::string& name = "");
        void DestroyGameObject(Core::GameObjectID id);
        void DestoryGO(GameObject* go);

        // 针对 GO 的循环机制
        void UpdateGameObjects(float deltaTime);
        void FixedUpdateGameObjects(float fixedDeltaTime);

        GameObject* GetGameObjectByID(Core::GameObjectID id);
        std::vector<GameObject> GetAllGameObjects();
        std::vector<Render::RenderObject> GetAllVisiableRenderObjects();

        void OnRuntimeStart();
        void OnRuntimeUpdate(float deltaTime);
        void OnRuntimeStop();

        // 编辑器更新：只处理编辑器摄像机、Gizmos，不跑物理
        void OnUpdateEditor(float dt);

        // TODO: 提供 resize 方法
        // void OnViewportResize(uint32_t width, uint32_t height);

        void RegisterRenderable(Renderable* comp);
        void UnregisterRenderable(Renderable* comp);

      private:
        struct GameObjectData
        {
            Core::GameObjectID id;
            GameObject* go;
        };
        // 缓存并且管理 scene 下的所有 GO
        std::unordered_map<Core::GameObjectID, std::unique_ptr<GameObject>> _objects;
        std::vector<GameObjectData> _runtimeSnapShot;
        std::mutex _renderMutex;
        std::vector<Renderable*> _renderables;
        bool _isRunning = false;

        std::unique_ptr<Physics::PhysicsScene> _physicsSceneOnRuntime;
    };
} // namespace ChikaEngine::Framework