#pragma once
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/subsystem/RenderSubsystem.h"
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

      private:
        std::unique_ptr<RenderSubsystem> _renderSubsystem = nullptr;

        // 已经在 Physics module 下做过一次封装, 直接使用, 不重复包装 subsystem
        std::unique_ptr<PhysicsSubsystem> _physicsSubsystem = nullptr;

      private:
        std::vector<std::unique_ptr<GameObject>> _gameobjects;              // 连续存储 GO
        std::unordered_map<Core::GameObjectID, GameObject*> _gameobjectMap; // 做查询使用
        SceneModes _mode = SceneModes::Play;
    };
} // namespace ChikaEngine::Framework