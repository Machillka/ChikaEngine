#pragma once

#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/io/MemoryStream.h"
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

        void Play(); // 进入 play mode
        void Stop(); // 从 play mode 退出

        // 生命周期
        void Start();
        void Update(float deltaTime);
        void FixedUpdate(float fixedDeltaTime);

        // 序列化方法
        template <typename Archive> void serialize(Archive& ar)
        {
            using namespace ChikaEngine::Serialization;

            size_t goCount = _gameobjects.size();
            ar.EnterArray("GameObjects", goCount);

            if constexpr (Archive::IsSaving)
            {
                for (auto& kv : _gameobjects)
                {
                    ar.EnterNode(nullptr);
                    kv.second->serialize(ar);
                    ar.LeaveNode();
                }
            }
            else if constexpr (Archive::IsLoading)
            {
                _gameobjects.clear();
                for (size_t i = 0; i < goCount; ++i)
                {
                    ar.EnterNode(nullptr);
                    auto go = std::make_unique<GameObject>("");
                    go->SetScene(this);

                    go->serialize(ar);
                    go->OnDeserialized();

                    // 放回 map 中
                    _gameobjects[go->GetID()] = std::move(go);
                    ar.LeaveNode();
                }
            }
            ar.LeaveArray();
        }

      private:
        SceneMode _mode;

        std::unique_ptr<Physics::PhysicsScene> _physicsScene;

        std::unordered_map<Core::GameObjectID, std::unique_ptr<GameObject>> _gameobjects;
        std::vector<Renderable*> _renderableObjects;

        // TODO: 管理生命周期 -> 线程安全
        std::vector<Component*> _startQueue;
        std::vector<Component*> _updateList;
        std::vector<GameObject*> _destroyQueue;

        // 序列化缓存
        std::unique_ptr<IO::MemoryStream> _editorSnapshot;
        void ClearRuntimeData();
    };
} // namespace ChikaEngine::Framework