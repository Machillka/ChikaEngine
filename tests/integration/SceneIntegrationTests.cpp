#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/prefab/Prefab.hpp"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/scene/SceneEvents.hpp"
#include "ChikaEngine/scene/SceneManager.hpp"
#include "ChikaEngine/scene/scene.hpp"
#include <cmath>
#include <iostream>

namespace
{
    class DestroyOwnerOnTick final : public ChikaEngine::Framework::Component
    {
      public:
        explicit DestroyOwnerOnTick(bool* destroyed) : destroyed(destroyed) {}

        void Tick(float) override
        {
            GetOwner()->GetScene()->DestroyGameObject(GetOwner()->GetID());
        }
        void OnDestroy() override
        {
            *destroyed = true;
        }

        bool* destroyed = nullptr;
    };

    bool NearlyEqual(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAILED: " << message << '\n';
        return 1;
    }
} // namespace

int main()
{
    using namespace ChikaEngine;

    Reflection::InitAllReflection();
    Core::UIDGenerator::Instance().Init(7);

    Framework::Scene scene;
    int createdEvents = 0;
    int destroyedEvents = 0;
    int modeEvents = 0;
    scene.GetEventBus().Subscribe<Framework::GameObjectCreatedEvent>([&](const auto&) { ++createdEvents; });
    scene.GetEventBus().Subscribe<Framework::GameObjectDestroyedEvent>([&](const auto&) { ++destroyedEvents; });
    scene.GetEventBus().Subscribe<Framework::SceneModeChangedEvent>([&](const auto&) { ++modeEvents; });

    const Core::GameObjectID parentId = scene.CreateGameobject("SnapshotTarget");
    const Core::GameObjectID childId = scene.CreateGameobject("Child");
    auto* parent = scene.GetGameObject(parentId);
    auto* child = scene.GetGameObject(childId);
    if (!parent || !parent->transform || !child || !child->transform)
        return Fail("scene did not create GameObjects with Transform");

    parent->transform->position = Math::Vector3(1.0f, 2.0f, 3.0f);
    child->transform->position = Math::Vector3(2.0f, 0.0f, 0.0f);
    if (!child->transform->SetParent(parent->transform))
        return Fail("transform hierarchy rejected valid parent");
    if (parent->transform->SetParent(child->transform))
        return Fail("transform hierarchy accepted a cycle");

    const auto worldPosition = child->transform->GetWorldPosition();
    if (!NearlyEqual(worldPosition.x, 3.0f) || !NearlyEqual(worldPosition.y, 2.0f) || !NearlyEqual(worldPosition.z, 3.0f))
        return Fail("child world transform does not include parent transform");

    Framework::Prefab prefab;
    if (!prefab.Capture(scene, parentId))
        return Fail("prefab capture failed");

    const auto prefabRootId = prefab.Instantiate(scene, "PrefabInstance");
    auto* prefabRoot = scene.GetGameObject(prefabRootId);
    if (!prefabRoot || prefabRootId == parentId || prefabRoot->transform->GetChildren().size() != 1)
        return Fail("prefab did not instantiate remapped hierarchy");
    const auto prefabChildId = prefabRoot->transform->GetChildren().front()->GetOwner()->GetID();

    if (!scene.StartPlayMode() || scene.GetMode() != Framework::SceneModes::Play)
        return Fail("scene did not enter Play mode");
    if (!scene.PausePlayMode() || scene.GetMode() != Framework::SceneModes::Paused || !scene.ResumePlayMode())
        return Fail("scene pause/resume transition failed");

    parent = scene.GetGameObject(parentId);
    parent->transform->position = Math::Vector3(9.0f, 9.0f, 9.0f);
    const auto runtimeObjectId = scene.CreateGameobject("RuntimeOnly");
    bool runtimeObjectDestroyed = false;
    scene.GetGameObject(runtimeObjectId)->AddComponent<DestroyOwnerOnTick>(&runtimeObjectDestroyed);
    scene.Tick(0.016f);
    if (scene.GetGameObject(runtimeObjectId) || !runtimeObjectDestroyed)
        return Fail("GameObject did not safely destroy itself during Tick");
    scene.DestroyGameObject(childId);
    if (!scene.StopPlayMode() || scene.GetMode() != Framework::SceneModes::Edit)
        return Fail("scene did not return to Edit mode");

    auto* restored = scene.GetGameObject(parentId);
    auto* restoredChild = scene.GetGameObject(childId);
    if (!restored || !restored->transform)
        return Fail("scene snapshot did not restore GameObject");

    const auto position = restored->transform->position;
    if (!NearlyEqual(position.x, 1.0f) || !NearlyEqual(position.y, 2.0f) || !NearlyEqual(position.z, 3.0f))
        return Fail("scene snapshot did not restore Transform");
    if (!restoredChild || restoredChild->transform->GetParent() != restored->transform)
        return Fail("scene snapshot did not restore Transform hierarchy");
    if (scene.GetGameObject(runtimeObjectId))
        return Fail("scene snapshot retained runtime-only GameObject");

    if (!scene.StartPlayMode() || !scene.StopPlayMode())
        return Fail("repeated Play mode session failed");

    scene.DestroyGameObject(prefabRootId);
    if (scene.GetGameObject(prefabRootId) || scene.GetGameObject(prefabChildId))
        return Fail("recursive hierarchy destruction failed");

    if (createdEvents < 5 || destroyedEvents < 2 || modeEvents < 8)
        return Fail("scene lifecycle events were not published");

    Framework::SceneManager sceneManager;
    if (!sceneManager.Initialize({}) || !sceneManager.CreateScene("Second") || !sceneManager.SetActiveScene("Second") || sceneManager.GetActiveSceneName() != "Second")
        return Fail("SceneManager create/switch failed");
    if (!sceneManager.DestroyScene("Second") || !sceneManager.GetActiveScene())
        return Fail("SceneManager destroy/fallback failed");
    sceneManager.Shutdown();

    if (!Reflection::TypeRegister::Instance().GetClassByName("Transform"))
        return Fail("reflection registry does not contain Transform");

    return 0;
}
