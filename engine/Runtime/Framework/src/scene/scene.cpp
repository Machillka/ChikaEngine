#include "ChikaEngine/scene/scene.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include <algorithm>
#include <memory>
#include <utility>
namespace ChikaEngine::Framework
{
    Scene::Scene(SceneMode mode) : _mode(mode)
    {
        Physics::PhysicsSystemDesc desc = {
            .backendType = Physics::PhysicsBackendTypes::Jolt,
        };
        // 生成对应实例
        _physicsScene = std::make_unique<Physics::PhysicsScene>(desc);
    }
    Scene::~Scene()
    {
        _gameobjects.clear();
    }

    Core::GameObjectID Scene::CreateGameobject(std::string name)
    {
        auto go = std::make_unique<GameObject>(name);
        go->SetScene(this);
        Core::GameObjectID id = go->GetID();
        _gameobjects[id] = std::move(go);
        return id;
    }

    GameObject* Scene::GetGameobject(Core::GameObjectID id)
    {
        return _gameobjects[id].get();
    }

    Physics::PhysicsScene* Scene::GetScenePhysics()
    {
        return _physicsScene.get();
    }

    void Scene::SyncTransform()
    {
        auto updatedTransformData = _physicsScene->PollTransform();
        for (auto& kv : updatedTransformData)
        {
            Core::GameObjectID id = kv.first;
            Physics::PhysicsTransform physicsTransform = kv.second;
            LOG_WARN("Scene", "id = {}, y = {}", id, physicsTransform.pos.y);
            _gameobjects[id]->transform->position = physicsTransform.pos;
            _gameobjects[id]->transform->rotation = physicsTransform.rot;
        }
    }

    void Scene::RegisterRenderable(Renderable* rend)
    {
        _renderableObjects.push_back(rend);
    }

    void Scene::UnregisterRenderable(Renderable* rend)
    {
        auto it = std::find(_renderableObjects.begin(), _renderableObjects.end(), rend);

        if (it != _renderableObjects.end())
            _renderableObjects.erase(it);
    }

    std::vector<Render::RenderObject> Scene::GetAllVisiableRenderObjects()
    {
        std::vector<Render::RenderObject> out;

        for (auto* r : _renderableObjects)
        {
            if (!r || (!r->IsVisible()))
                continue;
            out.push_back(r->BuildRenderObject());
        }

        return out;
    }

    // TODO: 把所有生命周期交给 world 实现, 因为 editor 没有具体的生命周期
    void Scene::Start()
    {
        for (auto& go : _gameobjects)
        {
            go.second->Start();
        }
    }

    void Scene::Update(float deltaTime)
    {
        for (auto& go : _gameobjects)
        {
            go.second->Update(deltaTime);
        }
    }

    void Scene::FixedUpdate(float fixedDeltaTime)
    {
        for (auto& go : _gameobjects)
        {
            go.second->FixedUpdate(fixedDeltaTime);
        }

        if (_mode == SceneMode::play)
        {
            _physicsScene->Tick(fixedDeltaTime);
            LOG_WARN("Scene", "Running Physics");
            SyncTransform();
        }
    }
} // namespace ChikaEngine::Framework