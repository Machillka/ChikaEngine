#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Collider.hpp"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/debug/Gizmo.hpp"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/io/MemoryStream.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/scene/scene.hpp"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace
{
    namespace Core = ChikaEngine::Core;
    namespace Framework = ChikaEngine::Framework;
    namespace Math = ChikaEngine::Math;
    namespace Physics = ChikaEngine::Physics;

    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    void TickOnePhysicsStep(Framework::Scene& scene)
    {
        scene.Tick(1.0f / 30.0f);
    }

    void TestFrameworkAuthoringLifecycle()
    {
        Framework::Scene scene;
        scene.Initialize({});

        const Core::GameObjectID staticId = scene.CreateGameobject("StaticCollider");
        auto* staticObject = scene.GetGameObject(staticId);
        auto* staticCollider = staticObject->AddComponent<Framework::Collider>();

        const Core::GameObjectID dynamicId = scene.CreateGameobject("DynamicCollider");
        auto* dynamicObject = scene.GetGameObject(dynamicId);
        auto* dynamicCollider = dynamicObject->AddComponent<Framework::Collider>();
        auto* dynamicRigidbody = dynamicObject->AddComponent<Framework::Rigidbody>();
        dynamicRigidbody->SetMass(3.0f);
        dynamicRigidbody->SetLinearDamping(0.15f);
        dynamicRigidbody->SetGravityFactor(0.0f);
        dynamicRigidbody->SetContinuousCollisionDetectionEnabled(true);
        dynamicRigidbody->SetAxisLockMask(Physics::PhysicsAxisLockRotationZ);

        const Core::GameObjectID kinematicId = scene.CreateGameobject("KinematicCollider");
        auto* kinematicObject = scene.GetGameObject(kinematicId);
        kinematicObject->AddComponent<Framework::Collider>();
        auto* kinematicRigidbody = kinematicObject->AddComponent<Framework::Rigidbody>();
        kinematicRigidbody->SetMotionType(Physics::MotionType::Kinematic);

        const Core::GameObjectID missingColliderId = scene.CreateGameobject("MissingCollider");
        auto* missingRigidbody = scene.GetGameObject(missingColliderId)->AddComponent<Framework::Rigidbody>();

        const Core::GameObjectID invalidId = scene.CreateGameobject("InvalidCollider");
        auto* invalidCollider = scene.GetGameObject(invalidId)->AddComponent<Framework::Collider>();
        invalidCollider->SetHalfExtents({ 0.0f, 0.5f, 0.5f });

        const Core::GameObjectID unsupportedId = scene.CreateGameobject("UnsupportedCollider");
        auto* unsupportedCollider = scene.GetGameObject(unsupportedId)->AddComponent<Framework::Collider>();
        unsupportedCollider->SetShapeType(Physics::ColliderShapeType::Capsule);

        Check(!missingRigidbody->GetAuthoringDiagnostic().empty(), "Rigidbody without Collider reports an authoring diagnostic");
        Check(!invalidCollider->GetAuthoringDiagnostic().empty(), "invalid Collider dimensions report an authoring diagnostic");
        Check(!unsupportedCollider->GetAuthoringDiagnostic().empty(), "unsupported Collider shape reports an authoring diagnostic");

        Check(scene.StartPlayMode(), "authoring fixture enters Play mode");
        TickOnePhysicsStep(scene);

        Physics::PhysicsScene* physics = scene.GetPhysicsSubsystem();
        const auto staticRecord = physics->GetBodyRecord(physics->GetBodyHandle(staticId));
        const auto dynamicRecord = physics->GetBodyRecord(physics->GetBodyHandle(dynamicId));
        const auto kinematicRecord = physics->GetBodyRecord(physics->GetBodyHandle(kinematicId));
        Check(staticRecord && staticRecord->motionType == Physics::MotionType::Static, "Collider without Rigidbody creates a Static body");
        Check(dynamicRecord && dynamicRecord->motionType == Physics::MotionType::Dynamic, "Collider with Dynamic Rigidbody creates a Dynamic body");
        Check(kinematicRecord && kinematicRecord->motionType == Physics::MotionType::Kinematic, "Collider with Kinematic Rigidbody creates a Kinematic body");
        Check(staticRecord && staticRecord->colliderHandle && dynamicRecord && dynamicRecord->colliderHandle, "runtime bodies expose valid Collider identities");
        Check(!physics->GetBodyHandle(missingColliderId), "Rigidbody without Collider does not create a default body");
        Check(!physics->GetBodyHandle(invalidId), "invalid Collider does not create a body");
        Check(!physics->GetBodyHandle(unsupportedId), "unsupported Collider does not create a body");

        const Physics::PhysicsBodyHandle oldBody = dynamicRecord ? dynamicRecord->handle : Physics::PhysicsBodyHandle::Invalid();
        const Physics::PhysicsColliderHandle stableCollider = dynamicRecord ? dynamicRecord->colliderHandle : Physics::PhysicsColliderHandle::Invalid();
        dynamicCollider = scene.GetGameObject(dynamicId)->GetComponent<Framework::Collider>();
        dynamicRigidbody = scene.GetGameObject(dynamicId)->GetComponent<Framework::Rigidbody>();
        dynamicCollider->SetHalfExtents({ 1.0f, 0.75f, 0.5f });
        TickOnePhysicsStep(scene);
        const auto rebuiltRecord = physics->GetBodyRecord(physics->GetBodyHandle(dynamicId));
        Check(rebuiltRecord && rebuiltRecord->handle != oldBody && rebuiltRecord->colliderHandle == stableCollider, "Collider edit replaces only the Body and preserves Collider identity");

        dynamicRigidbody->SetEnabled(false);
        TickOnePhysicsStep(scene);
        const auto staticFallback = physics->GetBodyRecord(physics->GetBodyHandle(dynamicId));
        Check(staticFallback && staticFallback->motionType == Physics::MotionType::Static, "disabled Rigidbody leaves the Collider as Static");
        dynamicRigidbody->SetEnabled(true);
        TickOnePhysicsStep(scene);
        const auto dynamicAgain = physics->GetBodyRecord(physics->GetBodyHandle(dynamicId));
        Check(dynamicAgain && dynamicAgain->motionType == Physics::MotionType::Dynamic, "re-enabled Rigidbody restores configured motion type");

        scene.GetGameObject(dynamicId)->RemoveComponent<Framework::Collider>();
        TickOnePhysicsStep(scene);
        Check(!physics->GetBodyHandle(dynamicId), "removing Collider destroys the body through the command buffer");
        Check(!dynamicRigidbody->GetAuthoringDiagnostic().empty(), "remaining Rigidbody reports that Collider was removed");

        Check(scene.StopPlayMode(), "authoring fixture exits Play mode");
        scene.Shutdown();
    }

    void TestShapeBuildGizmoAndQueryContract()
    {
        Framework::GameObject object(101, "ScaledCollider");
        object.transform->scale = { -2.0f, 3.0f, 4.0f };
        auto* collider = object.AddComponent<Framework::Collider>();
        collider->SetCenter({ 1.0f, 2.0f, 3.0f });
        collider->SetHalfExtents({ 0.5f, 1.0f, 1.5f });
        const Physics::ColliderShapeDesc worldShape = collider->BuildWorldShapeDesc();
        Check(NearlyEqual(worldShape.center.x, -2.0f) && NearlyEqual(worldShape.center.y, 6.0f) && NearlyEqual(worldShape.center.z, 12.0f), "Collider center applies signed world scale");
        Check(NearlyEqual(worldShape.halfExtents.x, 1.0f) && NearlyEqual(worldShape.halfExtents.y, 3.0f) && NearlyEqual(worldShape.halfExtents.z, 6.0f), "Box extents apply absolute world scale");

        ChikaEngine::Debug::Gizmo::Clear();
        collider->OnGizmo();
        Check(ChikaEngine::Debug::Gizmo::GetLineCount() == 12, "Box Collider gizmo uses the shared shape build rule");
        collider->SetShapeType(Physics::ColliderShapeType::Sphere);
        ChikaEngine::Debug::Gizmo::Clear();
        collider->OnGizmo();
        Check(ChikaEngine::Debug::Gizmo::GetLineCount() == 96, "Sphere Collider gizmo draws three complete circles");

        Physics::PhysicsSystemDesc systemDesc;
        systemDesc.initDesc.gravity = Math::Vector3::zero;
        systemDesc.initDesc.workerThreadCount = 1;
        Physics::PhysicsScene physics(systemDesc);
        Physics::PhysicsBodyCreateDesc desc;
        desc.ownerId = 202;
        desc.motionType = Physics::MotionType::Static;
        desc.shapeDesc.type = Physics::ColliderShapeType::Sphere;
        desc.shapeDesc.radius = 1.0f;
        desc.shapeDesc.center = { 2.0f, 0.0f, 0.0f };
        desc.queryEnabled = false;
        const auto created = physics.CreateBodyImmediate(desc);
        Check(static_cast<bool>(created), "query fixture creates centered Sphere body");
        Physics::RaycastHit hit;
        Check(!physics.Raycast({ 2.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, hit), "query-disabled Collider is excluded from raycast");
        desc.queryEnabled = true;
        Check(static_cast<bool>(physics.QueueRebuildBody(desc)), "query-enabled rebuild queues");
        physics.Tick(1.0f / 60.0f);
        Check(physics.Raycast({ 2.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f, hit) && hit.colliderHandle && hit.gameObjectId == desc.ownerId, "centered Collider participates in raycast and returns Collider identity");
    }

    void TestLegacySceneMigrationAndNewSchemaRoundTrip()
    {
        Framework::Scene authored;
        authored.Initialize({});
        const Core::GameObjectID id = authored.CreateGameobject("LegacySource");
        auto* object = authored.GetGameObject(id);
        object->AddComponent<Framework::Collider>();
        auto* rigidbody = object->AddComponent<Framework::Rigidbody>();
        rigidbody->SetMass(2.5f);

        ChikaEngine::IO::MemoryStream saveStream;
        authored.SaveToStream(saveStream);
        const auto& bytes = saveStream.GetRawData();
        nlohmann::json legacyJson = nlohmann::json::parse(bytes.begin(), bytes.end());
        auto& components = legacyJson["Scene"]["GameObjects"][0]["GameObject"]["Components"];
        for (auto& component : components)
        {
            const std::string typeName = component["TypeName"].get<std::string>();
            if (typeName.ends_with("Rigidbody"))
            {
                auto& data = component["CompData"];
                data["_colliderCenter"] = { { "x", 1.0f }, { "y", 2.0f }, { "z", 3.0f } };
                data["_colliderRadius"] = 0.75f;
                data["_colliderHeight"] = 2.25f;
                data["_friction"] = 0.8f;
            }
        }
        for (auto iterator = components.begin(); iterator != components.end();)
        {
            if ((*iterator)["TypeName"].get<std::string>().ends_with("Collider"))
                iterator = components.erase(iterator);
            else
                ++iterator;
        }

        const std::string legacyText = legacyJson.dump();
        ChikaEngine::IO::MemoryStream legacyStream(legacyText.data(), legacyText.size());
        Framework::Scene migrated;
        migrated.Initialize({});
        migrated.LoadFromStream(legacyStream);
        auto* migratedObject = migrated.GetGameObject(id);
        auto* migratedCollider = migratedObject ? migratedObject->GetComponent<Framework::Collider>() : nullptr;
        auto* migratedRigidbody = migratedObject ? migratedObject->GetComponent<Framework::Rigidbody>() : nullptr;
        Check(migratedCollider && migratedRigidbody, "legacy Rigidbody shape fields create an equivalent Collider component");
        Check(migratedCollider && migratedCollider->GetCenter() == Math::Vector3(1.0f, 2.0f, 3.0f) && NearlyEqual(migratedCollider->GetRadius(), 0.75f) && NearlyEqual(migratedCollider->GetHeight(), 2.25f) && NearlyEqual(migratedCollider->GetFriction(), 0.8f), "legacy Collider values migrate without loss");
        Check(migratedRigidbody && NearlyEqual(migratedRigidbody->GetMass(), 2.5f), "legacy migration preserves Rigidbody dynamics fields");

        ChikaEngine::IO::MemoryStream migratedSave;
        migrated.SaveToStream(migratedSave);
        const auto& migratedBytes = migratedSave.GetRawData();
        const nlohmann::json newSchema = nlohmann::json::parse(migratedBytes.begin(), migratedBytes.end());
        const auto& newComponents = newSchema["Scene"]["GameObjects"][0]["GameObject"]["Components"];
        int colliderCount = 0;
        bool legacyFieldSaved = false;
        for (const auto& component : newComponents)
        {
            const std::string typeName = component["TypeName"].get<std::string>();
            if (typeName.ends_with("Collider"))
                ++colliderCount;
            if (typeName.ends_with("Rigidbody"))
                legacyFieldSaved = component["CompData"].contains("_colliderCenter");
        }
        Check(colliderCount == 1 && !legacyFieldSaved, "saving a migrated Scene emits only the new Collider/Rigidbody schema");
        Check(Framework::Collider::GetClassName() && ChikaEngine::Reflection::TypeRegister::Instance().GetClassByName("Collider"), "Collider is registered for reflection construction");
    }
} // namespace

int main()
{
    ChikaEngine::Reflection::InitAllReflection();
    Core::UIDGenerator::Instance().Init(21);

    TestFrameworkAuthoringLifecycle();
    TestShapeBuildGizmoAndQueryContract();
    TestLegacySceneMigrationAndNewSchemaRoundTrip();

    if (g_failures == 0)
        std::cout << "Collider authoring checks passed\n";
    else
        std::cerr << g_failures << " Collider authoring check(s) failed\n";
    return g_failures == 0 ? 0 : 1;
}
