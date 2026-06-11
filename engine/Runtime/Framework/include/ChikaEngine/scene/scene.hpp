#pragma once
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/base/FixedStepAccumulator.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/event/EventBus.hpp"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/SceneEvents.hpp"
#include "ChikaEngine/subsystem/AnimationSubsystem.hpp"
#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/subsystem/RenderSubsystem.h"
#include "ChikaEngine/io/IStream.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace ChikaEngine::Framework
{
    struct SceneCreateInfo
    {
        Render::Renderer* renderInstance = nullptr;
        float fixedDeltaTime = 1.0f / 60.0f;
        uint32_t maxPhysicsStepsPerFrame = 4;
    };

    class Scene
    {
        friend class Prefab;

      public:
        Scene() = default;
        ~Scene();

        void SaveToStream(IO::IStream& stream) const;
        void LoadFromStream(IO::IStream& stream);

        // 会被分配到自定义序列化方法
        template <typename Archive> void serialize(Archive& ar)
        {
            size_t goCount = _gameobjects.size();
            ar.EnterArray("GameObjects", goCount);

            if constexpr (Archive::IsSaving)
            {
                for (auto& go : _gameobjects)
                {
                    ar.EnterNode(nullptr);
                    ar("GameObject", *go);
                    ar.LeaveNode();
                }
            }
            else if constexpr (Archive::IsLoading)
            {
                ClearGameObjects();

                for (size_t i = 0; i < goCount; ++i)
                {
                    ar.EnterNode(nullptr);
                    auto go = std::make_unique<GameObject>();
                    go->SetScene(this);
                    ar("GameObject", *go);

                    _gameobjectMap[go->GetID()] = go.get();
                    _gameobjects.emplace_back(std::move(go));
                    ar.LeaveNode();
                }
            }

            ar.LeaveArray();
        }
        Core::GameObjectID CreateGameobject(std::string name);
        bool DestroyGameObject(Core::GameObjectID id);
        GameObject* GetGameObject(std::string name);
        GameObject* GetGameObject(Core::GameObjectID);

        RenderSubsystem* GetRenderSubsystem();
        Physics::PhysicsScene* GetPhysicsSubsystem();
        const std::vector<std::unique_ptr<GameObject>>& GetAllGameobjects() const;

        void Initialize(const SceneCreateInfo& createInfo);
        void Shutdown();
        void ChangeSceneMode(SceneModes newMode);
        SceneModes GetMode() const
        {
            return _mode;
        }
        bool IsEditing() const
        {
            return _mode == SceneModes::Edit;
        }
        bool IsPlaying() const
        {
            return _mode == SceneModes::Play;
        }
        bool IsPaused() const
        {
            return _mode == SceneModes::Paused;
        }
        EventBus& GetEventBus()
        {
            return _eventBus;
        }
        const EventBus& GetEventBus() const
        {
            return _eventBus;
        }

      public:
        void Tick(float deltaTime);

        // 用于反序列化的时候恢复指针等
        void OnDeserialized();

        bool StartPlayMode();
        bool PausePlayMode();
        bool ResumePlayMode();
        bool StopPlayMode();
        void FlushPendingChanges();

      private:
        void SetMode(SceneModes mode);
        void ClearGameObjects();
        void CollectDestroyHierarchy(GameObject& object);
        std::vector<GameObject*> SnapshotGameObjects() const;

        std::unique_ptr<RenderSubsystem> _renderSubsystem = nullptr;
        std::unique_ptr<PhysicsSubsystem> _physicsSubsystem = nullptr;
        std::unique_ptr<AnimationSubsystem> _animationSubsystem = nullptr;

      private:
        std::vector<std::unique_ptr<GameObject>> _gameobjects;              // 连续存储 GO
        std::unordered_map<Core::GameObjectID, GameObject*> _gameobjectMap; // 做查询使用
        std::unordered_set<Core::GameObjectID> _pendingDestroy;
        SceneModes _mode = SceneModes::Edit;
        std::optional<SceneModes> _requestedMode;
        bool _isTicking = false;
        bool _isClearing = false;

        std::vector<uint8_t> _playBackup;
        Core::FixedStepAccumulator _physicsStepper;
        EventBus _eventBus;
    };
} // namespace ChikaEngine::Framework
