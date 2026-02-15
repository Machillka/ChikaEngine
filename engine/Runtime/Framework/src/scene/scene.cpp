#include "ChikaEngine/scene/scene.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/io/FileStream.h"
#include "ChikaEngine/io/IStream.h"
#include "ChikaEngine/io/MemoryStream.h"
#include "ChikaEngine/serialization/BinarySaveArchive.h"
#include "ChikaEngine/serialization/JsonLoadArchive.h"
#include "ChikaEngine/serialization/JsonSaveArchive.h"

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

    Physics::PhysicsScene* Scene::GetPhysicsScene()
    {
        return _physicsScene.get();
    }

    void Scene::ClearAllObjects()
    {
        _objects.clear();
    }

    void Scene::OnStart()
    {
        // 如果有数据流输入 就说明需要 copy 一份 scene 再开始运行
        for (auto& kv : _objects)
        {
            kv.second->Start();
        }
    }
    void Scene::OnUpdate(float deltaTime)
    {
        for (auto& kv : _objects)
        {
            kv.second->Update(deltaTime);
        }
    }
    void Scene::OnFixedUpdate(float fixedDeltaTime)
    {
        for (auto& kv : _objects)
        {
            kv.second->FixedUpdate(fixedDeltaTime);
        }
    }
    void Scene::OnPhysicsUpdate(float fixedDeltaTime)
    {
        // 更新物理系统和 transform
    }

    void Scene::OnEnd()
    {
        {
            IO::FileStream jsonOutputStream("Seetings/scene.json", IO::Mode::Write);
            Serialization::JsonSaveArchive archive(jsonOutputStream);
            Serialize(archive); // 自动根据编译逻辑执行序列化操作
        }

        {
            IO::MemoryStream memoryOutputStream;
            Serialization::BinarySaveArchive archive(memoryOutputStream);
            Serialize(archive);
        }
    }

} // namespace ChikaEngine::Framework