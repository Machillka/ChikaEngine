#pragma once

#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include <memory>
#include <unordered_map>
#include <vector>
namespace ChikaEngine::Framework
{
    enum class SceneMode
    {
        edit,
        play
    };

    class Scene
    {
      public:
        Scene(SceneMode mode);
        ~Scene();

        SceneMode GetMode() const
        {
            return _mode;
        }

        Core::GameObjectID CreateGameobject(std::string name = "gameobject");
        GameObject* GetGameobject(Core::GameObjectID id);

        // 物理相关
        Physics::PhysicsScene* GetScenePhysics();
        void SyncTransform();

        // 渲染相关
        void RegisterRenderable(Renderable* rend);
        void UnregisterRenderable(Renderable* rend);
        std::vector<Render::RenderObject> GetAllVisiableRenderObjects();

        // 生命周期
        void Start();
        void Update(float deltaTime);
        void FixedUpdate(float fixedDeltaTime);

        // 序列化方法
        void Serilize();

      private:
        SceneMode _mode;

        std::unique_ptr<Physics::PhysicsScene> _physicsScene;

        std::unordered_map<Core::GameObjectID, std::unique_ptr<GameObject>> _gameobjects;
        std::vector<Renderable*> _renderableObjects;

        // TODO: 管理生命周期 -> 线程安全
        std::vector<Component*> _startQueue;
        std::vector<Component*> _updateList;
        std::vector<GameObject*> _destroyQueue;
    };
} // namespace ChikaEngine::Framework