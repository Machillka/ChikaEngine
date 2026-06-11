#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/SceneEvents.hpp"

#include "ChikaEngine/subsystem/AnimationSubsystem.hpp"
#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/subsystem/RenderSubsystem.h"
#include "ChikaEngine/serialization/JsonSaveArchive.h"
#include "ChikaEngine/serialization/JsonLoadArchive.h"
#include "ChikaEngine/io/MemoryStream.h"
#include <memory>
#include <optional>
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

        _physicsSubsystem = std::make_unique<PhysicsSubsystem>(this);
        if (createInfo.renderInstance)
        {
            _renderSubsystem = std::make_unique<RenderSubsystem>(this, createInfo.renderInstance);
            _animationSubsystem = std::make_unique<AnimationSubsystem>(this, createInfo.renderInstance->GetAssetManager());
        }
        _physicsStepper.Configure(createInfo.fixedDeltaTime, createInfo.maxPhysicsStepsPerFrame);

        _mode = SceneModes::Edit;
    }

    void Scene::Shutdown()
    {
        if (_mode == SceneModes::Play || _mode == SceneModes::Paused)
        {
            for (auto& gameObject : _gameobjects)
                gameObject->EndPlay();
        }

        ClearGameObjects();
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
        _requestedMode.reset();
        _mode = SceneModes::Edit;
        _eventBus.Clear();
    }

    Core::GameObjectID Scene::CreateGameobject(std::string name)
    {
        if (_isClearing)
            return Core::InvalidGameObjectID;

        Core::GameObjectID id = Core::UIDGenerator::Instance().Generate();

        auto go = std::make_unique<GameObject>(id, name, this);

        auto* created = go.get();
        _gameobjectMap[id] = created;

        _gameobjects.emplace_back(std::move(go));

        _eventBus.Publish(GameObjectCreatedEvent{ .scene = this, .gameObjectId = id });
        if (_mode == SceneModes::Play)
            created->BeginPlay();
        return id;
    }

    bool Scene::DestroyGameObject(Core::GameObjectID id)
    {
        auto* object = GetGameObject(id);
        if (!object)
            return false;

        CollectDestroyHierarchy(*object);
        if (!_isTicking)
            FlushPendingChanges();
        return true;
    }

    void Scene::CollectDestroyHierarchy(GameObject& object)
    {
        if (_pendingDestroy.contains(object.GetID()))
            return;

        _pendingDestroy.insert(object.GetID());
        object.MarkPendingDestroy();

        if (!object.transform)
            return;

        const auto children = object.transform->GetChildren();
        for (auto* child : children)
        {
            if (child && child->GetOwner())
                CollectDestroyHierarchy(*child->GetOwner());
        }
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

        if (_isTicking)
        {
            _requestedMode = newMode;
            return;
        }

        switch (newMode)
        {
        case SceneModes::Play:
            if (_mode == SceneModes::Paused)
                ResumePlayMode();
            else
                StartPlayMode();
            break;
        case SceneModes::Paused:
            PausePlayMode();
            break;
        case SceneModes::Edit:
            StopPlayMode();
            break;
        case SceneModes::EnteringPlay:
        case SceneModes::ExitingPlay:
            break;
        }
    }

    void Scene::Tick(float deltaTime)
    {
        FlushPendingChanges();

        if (_mode == SceneModes::Play)
        {
            _isTicking = true;
            const auto gameObjects = SnapshotGameObjects();

            _physicsStepper.Consume(deltaTime,
                                    [this, &gameObjects](float fixedDeltaTime)
                                    {
                                        for (auto* gameObject : gameObjects)
                                            gameObject->FixedTick(fixedDeltaTime);
                                        if (_physicsSubsystem)
                                        {
                                            _physicsSubsystem->Tick(fixedDeltaTime);
                                            _physicsSubsystem->SyncTransform();
                                        }
                                    });

            for (auto* gameObject : gameObjects)
                gameObject->Tick(deltaTime);

            if (_animationSubsystem)
                _animationSubsystem->Tick(deltaTime);

            for (auto* gameObject : gameObjects)
                gameObject->LateTick(deltaTime);

            _isTicking = false;
        }

        if (_mode == SceneModes::Edit || _mode == SceneModes::Paused)
        {
            for (auto& go : _gameobjects)
            {
                for (auto& comp : go->GetAllComponents())
                    comp->OnGizmo();
            }
        }

        if (_renderSubsystem)
            _renderSubsystem->Tick(deltaTime);

        FlushPendingChanges();

        if (_requestedMode)
        {
            const auto requested = *_requestedMode;
            _requestedMode.reset();
            ChangeSceneMode(requested);
        }
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

        for (auto& go : _gameobjects)
        {
            if (go->transform)
                go->transform->ResetHierarchyLinks();
        }

        for (auto& go : _gameobjects)
        {
            if (go->transform)
                go->transform->ResolveHierarchy(*this);
        }

        for (auto& go : _gameobjects)
            go->RefreshActiveInHierarchy();
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

    void Scene::SetMode(SceneModes mode)
    {
        if (_mode == mode)
            return;

        const auto previous = _mode;
        _mode = mode;
        _physicsStepper.Reset();
        _eventBus.Publish(SceneModeChangedEvent{ .scene = this, .previousMode = previous, .currentMode = mode });
    }

    bool Scene::StartPlayMode()
    {
        if (_isTicking)
        {
            _requestedMode = SceneModes::Play;
            return true;
        }
        if (_mode != SceneModes::Edit)
            return false;

        IO::MemoryStream mem;
        SaveToStream(mem);
        _playBackup = mem.GetRawData();

        SetMode(SceneModes::EnteringPlay);
        _isTicking = true;
        for (auto* gameObject : SnapshotGameObjects())
            gameObject->BeginPlay();
        _isTicking = false;
        FlushPendingChanges();
        SetMode(SceneModes::Play);
        return true;
    }

    bool Scene::PausePlayMode()
    {
        if (_mode != SceneModes::Play)
            return false;
        SetMode(SceneModes::Paused);
        return true;
    }

    bool Scene::ResumePlayMode()
    {
        if (_mode != SceneModes::Paused)
            return false;
        SetMode(SceneModes::Play);
        return true;
    }

    bool Scene::StopPlayMode()
    {
        if (_isTicking)
        {
            _requestedMode = SceneModes::Edit;
            return true;
        }
        if (_mode != SceneModes::Play && _mode != SceneModes::Paused)
            return false;

        if (_playBackup.empty())
            return false;

        SetMode(SceneModes::ExitingPlay);
        _isTicking = true;
        for (auto* gameObject : SnapshotGameObjects())
            gameObject->EndPlay();
        _isTicking = false;
        FlushPendingChanges();

        IO::MemoryStream mem(_playBackup.data(), _playBackup.size());
        LoadFromStream(mem);

        _playBackup.clear();
        SetMode(SceneModes::Edit);
        return true;
    }

    void Scene::FlushPendingChanges()
    {
        for (auto& gameObject : _gameobjects)
            gameObject->FlushPendingComponentRemovals();

        if (_isTicking || _pendingDestroy.empty())
            return;

        for (auto& gameObject : _gameobjects)
        {
            if (!_pendingDestroy.contains(gameObject->GetID()))
                continue;

            const auto id = gameObject->GetID();
            gameObject->PrepareDestroy();
            _gameobjectMap.erase(id);
            _eventBus.Publish(GameObjectDestroyedEvent{ .scene = this, .gameObjectId = id });
        }

        std::erase_if(_gameobjects, [this](const auto& gameObject) { return _pendingDestroy.contains(gameObject->GetID()); });
        _pendingDestroy.clear();
    }

    void Scene::ClearGameObjects()
    {
        _isClearing = true;
        for (auto* gameObject : SnapshotGameObjects())
            gameObject->PrepareDestroy();
        _gameobjects.clear();
        _gameobjectMap.clear();
        _pendingDestroy.clear();
        _isClearing = false;
    }

    std::vector<GameObject*> Scene::SnapshotGameObjects() const
    {
        std::vector<GameObject*> snapshot;
        snapshot.reserve(_gameobjects.size());
        for (const auto& gameObject : _gameobjects)
            snapshot.push_back(gameObject.get());
        return snapshot;
    }
} // namespace ChikaEngine::Framework
