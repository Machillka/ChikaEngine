#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/scene/scene.hpp"
#include <cmath>
#include <iostream>

namespace
{
    bool NearlyEqual(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }
} // namespace

int main()
{
    using namespace ChikaEngine;

    Reflection::InitAllReflection();
    Core::UIDGenerator::Instance().Init(7);

    Framework::Scene scene;
    scene.ChangeSceneMode(Framework::SceneModes::Edit);

    const Core::GameObjectID objectId = scene.CreateGameobject("SnapshotTarget");
    auto* object = scene.GetGameObject(objectId);
    if (!object || !object->transform)
    {
        std::cerr << "FAILED: scene did not create a GameObject with Transform\n";
        return 1;
    }

    object->transform->position = Math::Vector3(1.0f, 2.0f, 3.0f);
    scene.StartPlayMode();
    object->transform->position = Math::Vector3(9.0f, 9.0f, 9.0f);
    scene.StopPlayMode();

    auto* restored = scene.GetGameObject(objectId);
    if (!restored || !restored->transform)
    {
        std::cerr << "FAILED: scene snapshot did not restore GameObject\n";
        return 1;
    }

    const auto position = restored->transform->position;
    if (!NearlyEqual(position.x, 1.0f) || !NearlyEqual(position.y, 2.0f) || !NearlyEqual(position.z, 3.0f))
    {
        std::cerr << "FAILED: scene snapshot did not restore Transform\n";
        return 1;
    }

    if (!Reflection::TypeRegister::Instance().GetClassByName("Transform"))
    {
        std::cerr << "FAILED: reflection registry does not contain Transform\n";
        return 1;
    }

    return 0;
}
