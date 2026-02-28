#include "ChikaEngine/scene/scene.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/io/FileStream.h"
#include "ChikaEngine/io/IStream.h"
#include "ChikaEngine/serialization/BinaryLoadArchive.h"
#include "ChikaEngine/serialization/BinarySaveArchive.h"
#include "ChikaEngine/serialization/JsonSaveArchive.h"
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
        auto it = _gameobjects.find(id);
        if (it != _gameobjects.end())
        {
            return it->second.get();
        }
        return nullptr;
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

            auto it = _gameobjects.find(id);
            if (it == _gameobjects.end())
                continue;

            GameObject* go = it->second.get();

            Physics::PhysicsTransform physicsTransform = kv.second;

            if (go && go->transform)
            {
                go->transform->position = physicsTransform.pos;
                go->transform->rotation = physicsTransform.rot;
            }
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

    void Scene::Play()
    {
        if (_mode == SceneMode::play)
            return;

        LOG_INFO("Scene", "Entering play mode...");

        {
            IO::FileStream debugFile("debug_pre_play.json", IO::Mode::Write);

            Serialization::JsonSaveArchive jsonAr(debugFile);
            jsonAr("scene", *this);
            LOG_INFO("Debug", "Snapshot saved to 'debug_pre_play.json'");
        }

        _editorSnapshot = std::make_unique<IO::MemoryStream>();
        Serialization::BinarySaveArchive saveAr(*_editorSnapshot);
        saveAr(*this);
        _mode = SceneMode::play;
        Start();
    }

    void Scene::Stop()
    {
        if (_mode == SceneMode::edit)
            return;

        LOG_INFO("Scene", "Stopping play mode, restoring Editor state...");

        _mode = SceneMode::edit;

        ClearRuntimeData();
        // {
        //     IO::FileStream debugFile("debug_pre_play.json", IO::Mode::Read);

        //     Serialization::JsonLoadArchive jsonAr(debugFile);
        //     jsonAr("scene", *this);
        // }
        if (_editorSnapshot)
        {
            _editorSnapshot->FlipToRead();

            Serialization::BinaryLoadArchive loadAr(*_editorSnapshot);
            loadAr(*this);

            // NOTE: 可以遍历来重新注册 go 等东西
            // NOTE: 把注册下放给 component
            LOG_INFO("Scene", "Editor state restored successfully.");
        }
        else
        {
            LOG_ERROR("Scene", "Fatal: No editor snapshot found!");
        }
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
    void Scene::ClearRuntimeData()
    {
        _gameobjects.clear();

        _startQueue.clear();
        _updateList.clear();
        _destroyQueue.clear();

        _renderableObjects.clear();
        _physicsScene.reset();

        Physics::PhysicsSystemDesc desc = {.backendType = Physics::PhysicsBackendTypes::Jolt};
        _physicsScene = std::make_unique<Physics::PhysicsScene>(desc);
    }
} // namespace ChikaEngine::Framework