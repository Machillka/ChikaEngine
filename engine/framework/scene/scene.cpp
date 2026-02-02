#include "framework/scene/scene.h"

#include "debug/log_macros.h"
#include "framework/gameobject/GameObject.h"

#include <memory>
#include <string>
#include <utility>

namespace ChikaEngine::Framework
{
    Scene& Scene::Instance()
    {
        static Scene instance;
        return instance;
    }

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

    void Scene::Tick(float dt)
    {
        // TODO: 添加Update
    }

} // namespace ChikaEngine::Framework