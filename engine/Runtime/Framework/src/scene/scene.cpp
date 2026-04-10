#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/gameobject/GameObject.h"

#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/subsystem/RenderSubsystem.h"
#include <memory>
#include <utility>
#include <vector>

namespace ChikaEngine::Framework
{

    void Scene::Initialize(const SceneCreateInfo& createInfo)
    {
        // 初始化 render sub sysytem
        _renderSubsystem = std::make_unique<RenderSubsystem>(this, createInfo.renderInstance);
        _physicsSubsystem = std::make_unique<PhysicsSubsystem>(this);
    }

    Core::GameObjectID Scene::CreateGameobject(std::string name)
    {
        Core::GameObjectID id = Core::UIDGenerator::Instance().Generate();

        auto go = std::make_unique<GameObject>(id, name, this);

        _gameobjectMap[id] = go.get();

        _gameobjects.emplace_back(std::move(go));

        return id;
    }

    GameObject* Scene::GetGameObject(std::string name)
    {
        for (auto& go : _gameobjects)
        {
            if (go->GetName() == name)
                return go.get();
        }
        return nullptr;
    }

    GameObject* Scene::GetGameObject(Core::GameObjectID id)
    {
        auto it = _gameobjectMap.find(id);
        if (it != _gameobjectMap.end())
            return it->second;
        return nullptr;
    }

    void Scene::ChangeSceneMode(SceneModes newMode)
    {
        _mode = newMode;
    }

    void Scene::Tick(float deltaTime)
    {
        if (_mode == SceneModes::Play)
        {
            for (auto& go : _gameobjects)
                go->Tick(deltaTime);

            // TODO: 设计 Should Tick 方法进行检测
            if (_physicsSubsystem)
            {
                _physicsSubsystem->Tick(deltaTime);
                _physicsSubsystem->SyncTransform();
            }
        }

        if (_mode == SceneModes::Edit)
        {
            for (auto& go : _gameobjects)
            {
                for (auto& comp : go->GetAllComponents())
                    comp->OnGizmo();
            }
        }

        if (_renderSubsystem)
            _renderSubsystem->Tick(deltaTime);
    }

    RenderSubsystem* Scene::GetRenderSubsystem()
    {
        return _renderSubsystem.get();
    }

    const std::vector<std::unique_ptr<GameObject>>& Scene::GetAllGameobjects() const
    {
        return _gameobjects;
    }

    Physics::PhysicsScene* Scene::GetPhysicsSubsystem()
    {
        return _physicsSubsystem->GetPhysicsInstace();
    }
} // namespace ChikaEngine::Framework