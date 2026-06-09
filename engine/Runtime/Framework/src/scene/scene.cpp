#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"

#include "ChikaEngine/subsystem/AnimationSubsystem.hpp"
#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/subsystem/RenderSubsystem.h"
#include "ChikaEngine/serialization/JsonSaveArchive.h"
#include "ChikaEngine/serialization/JsonLoadArchive.h"
#include "ChikaEngine/io/MemoryStream.h"
#include <memory>
#include <utility>
#include <vector>

namespace ChikaEngine::Framework
{
    Scene::~Scene()
    {
        Shutdown();
    }

    void Scene::Initialize(const SceneCreateInfo& createInfo)
    {
        Shutdown();

        if (!createInfo.renderInstance)
        {
            LOG_ERROR("Scene", "SceneCreateInfo::renderInstance must not be null");
            return;
        }

        // 初始化 sub sysytem
        _renderSubsystem = std::make_unique<RenderSubsystem>(this, createInfo.renderInstance);
        _physicsSubsystem = std::make_unique<PhysicsSubsystem>(this);
        _animationSubsystem = std::make_unique<AnimationSubsystem>(this, createInfo.renderInstance->GetAssetManager());
        _physicsStepper.Configure(createInfo.fixedDeltaTime, createInfo.maxPhysicsStepsPerFrame);

        _mode = SceneModes::Edit;
    }

    void Scene::Shutdown()
    {
        _gameobjects.clear();
        _gameobjectMap.clear();
        _playBackup.clear();
        _physicsStepper.Reset();

        if (_animationSubsystem)
            _animationSubsystem->Cleanup();
        if (_renderSubsystem)
            _renderSubsystem->Cleanup();
        if (_physicsSubsystem)
            _physicsSubsystem->Cleanup();

        _animationSubsystem.reset();
        _renderSubsystem.reset();
        _physicsSubsystem.reset();
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
        if (_mode == newMode)
            return;
        _mode = newMode;
        _physicsStepper.Reset();
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
                _physicsStepper.Consume(deltaTime,
                                        [this](float fixedDeltaTime)
                                        {
                                            _physicsSubsystem->Tick(fixedDeltaTime);
                                            _physicsSubsystem->SyncTransform();
                                        });
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

        if (_animationSubsystem)
            _animationSubsystem->Tick(deltaTime);

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
        return _physicsSubsystem ? _physicsSubsystem->GetPhysicsInstace() : nullptr;
    }

    void Scene::OnDeserialized()
    {
        _gameobjectMap.clear();
        for (auto& go : _gameobjects)
        {
            go->SetScene(this);
            _gameobjectMap[go->GetID()] = go.get();
            go->OnDeserialized();
        }
    }

    void Scene::SaveToStream(IO::IStream& stream) const
    {
        Serialization::JsonSaveArchive ar(stream);
        ar.EnterNode("Scene");
        const_cast<Scene*>(this)->serialize(ar);
        ar.LeaveNode();
    }
    void Scene::LoadFromStream(IO::IStream& stream)
    {
        Serialization::JsonLoadArchive ar(stream);
        ar.EnterNode("Scene");
        serialize(ar);
        ar.LeaveNode();
        OnDeserialized();
    }

    void Scene::StartPlayMode()
    {
        if (_mode == SceneModes::Play)
            return;

        IO::MemoryStream mem;
        SaveToStream(mem);

        // 保存 snapshot
        _playBackup = mem.GetRawData();

        _mode = SceneModes::Play;
        _physicsStepper.Reset();
    }

    void Scene::StopPlayMode()
    {
        if (_mode == SceneModes::Edit)
            return;

        if (_playBackup.empty())
            return;

        IO::MemoryStream mem(_playBackup.data(), _playBackup.size());
        LoadFromStream(mem);

        _playBackup.clear();
        _mode = SceneModes::Edit;
        _physicsStepper.Reset();
    }
} // namespace ChikaEngine::Framework
