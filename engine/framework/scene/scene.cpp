#include "framework/scene/scene.h"

#include "debug/log_macros.h"
#include "framework/gameobject/GameObject.h"
#include "render/renderobject.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace ChikaEngine::Framework
{
    Scene& Scene::Instance()
    {
        static Scene instance;
        return instance;
    }

    // TODO: 实现通过名字查找 go

    // 在创建的时候直接注册到 Scene 中
    GameObject* Scene::CreateGO(const std::string& name)
    {
        auto go = std::make_unique<GameObject>();
        if (!name.empty())
        {
            go->SetName(name);
        }
        auto ptr = go.get();

        _objects[go->GetID()] = std::move(go);
        LOG_INFO("Scene", "Create GO id = {}, name = {}", ptr->GetID(), ptr->GetName());

        return ptr;
    }

    GameObject* Scene::GetGOByID(GameObjectID id)
    {
        auto go = _objects.find(id);
        return go != _objects.end() ? go->second.get() : nullptr;
    }

    // TODO[x]: 添加Update
    void Scene::Update(float deltaTime)
    {
        std::vector<GameObject*> snapshot;
        snapshot.reserve(_objects.size());
        for (auto& kv : _objects)
            snapshot.push_back(kv.second.get());

        for (auto* go : snapshot)
        {
            if (go->IsActive())
                go->Update(deltaTime);
        }
    }

    void Scene::FixedUpdate(float fixedDeltaTime)
    {
        std::vector<GameObject*> snapshot;
        snapshot.reserve(_objects.size());
        for (auto& kv : _objects)
            snapshot.push_back(kv.second.get());

        for (auto* go : snapshot)
        {
            if (go->IsActive())
                go->FixedUpdate(fixedDeltaTime);
        }
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

} // namespace ChikaEngine::Framework