#pragma once
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/subsystem/AnimationSubsystem.hpp"
#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/subsystem/RenderSubsystem.h"
#include "ChikaEngine/io/IStream.h"
#include <cstddef>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
namespace ChikaEngine::Framework
{
    enum class SceneModes
    {
        Play,
        Edit,
    };

    struct SceneCreateInfo
    {
        Render::Renderer* renderInstance;
    };

    class Scene
    {
      public:
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
                _gameobjects.clear();
                _gameobjectMap.clear();

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
        GameObject* GetGameObject(std::string name);
        GameObject* GetGameObject(Core::GameObjectID);

        RenderSubsystem* GetRenderSubsystem();
        Physics::PhysicsScene* GetPhysicsSubsystem();
        const std::vector<std::unique_ptr<GameObject>>& GetAllGameobjects() const;

        void Initialize(const SceneCreateInfo& createInfo);
        void ChangeSceneMode(SceneModes newMode);

      public:
        void Tick(float deltaTime);

        // 用于反序列化的时候恢复指针等
        void OnDeserialized();

        void StartPlayMode();
        void StopPlayMode();

      private:
        std::unique_ptr<RenderSubsystem> _renderSubsystem = nullptr;
        std::unique_ptr<PhysicsSubsystem> _physicsSubsystem = nullptr;
        std::unique_ptr<AnimationSubsystem> _animationSubsystem = nullptr;

      private:
        std::vector<std::unique_ptr<GameObject>> _gameobjects;              // 连续存储 GO
        std::unordered_map<Core::GameObjectID, GameObject*> _gameobjectMap; // 做查询使用
        SceneModes _mode = SceneModes::Play;

        std::vector<uint8_t> _playBackup;
    };
} // namespace ChikaEngine::Framework