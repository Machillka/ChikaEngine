#include "ChikaEngine/scene/scene.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/io/MemoryStream.h"
#include "ChikaEngine/serialization/JsonArchive.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace ChikaEngine::Framework
{
    Scene::Scene() = default;
    Scene::~Scene() = default;

    // TODO: 实现通过名字查找 go
    // 在创建的时候直接注册到 Scene 中
    GameObject* Scene::CreateGameObject(const std::string& name)
    {
        auto go = std::make_unique<GameObject>(name);
        if (!name.empty())
        {
            go->SetName(name);
        }
        auto ptr = go.get();

        _objects[go->GetID()] = std::move(go);
        LOG_INFO("Scene", "Create GO id = {}, name = {}", ptr->GetID(), ptr->GetName());

        return ptr;
    }

    GameObject* Scene::GetGameObjectByID(Core::GameObjectID id)
    {
        auto go = _objects.find(id);
        return go != _objects.end() ? go->second.get() : nullptr;
    }

    void Scene::RegisterRenderable(Renderable* ro)
    {
        if (!ro)
            return;
        std::lock_guard lock(_renderMutex);
        auto it = std::find(_renderables.begin(), _renderables.end(), ro);

        // 不存在
        if (it == _renderables.end())
            _renderables.push_back(ro);
    }

    void Scene::UnregisterRenderable(Renderable* ro)
    {
        if (!ro)
            return;
        std::lock_guard lock(_renderMutex);
        auto it = std::find(_renderables.begin(), _renderables.end(), ro);

        // 存在
        if (it != _renderables.end())
            _renderables.erase(it);
    }

    std::vector<Render::RenderObject> Scene::GetAllVisiableRenderObjects()
    {
        std::lock_guard lock(_renderMutex);
        std::vector<Render::RenderObject> out;

        for (auto* r : _renderables)
        {
            if (!r || (!r->IsVisible()))
                continue;
            out.push_back(r->BuildRenderObject());
        }

        return out;
    }

    void Scene::OnRuntimeStart()
    {
        if (_isRunning)
            return;

        // 1. 【快照】使用二进制流将当前场景存入内存
        LOG_INFO("Scene", "Taking Scene Snapshot for Play Mode...");
        IO::MemoryStream ms;
        {
            Serialization::JsonOutputArchive ar(ms);
            this->Serialize(ar); // 执行序列化
        }
        // _snapshotBuffer = ms.Read();

        // 2. 初始化物理场景
        Physics::PhysicsSystemDesc desc = {};
        _physicsSceneOnRuntime = std::make_unique<Physics::PhysicsScene>(desc);

        _isRunning = true;
    }
    void Scene::OnRuntimeUpdate(float deltaTime) {}
    void Scene::OnRuntimeStop() {}

    Physics::PhysicsScene* Scene::GetRuntimeScenePhysics()
    {
        return _physicsSceneOnRuntime.get();
    }
} // namespace ChikaEngine::Framework