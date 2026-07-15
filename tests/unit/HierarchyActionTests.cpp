#include "HierarchyActions.hpp"

#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/scene/scene.hpp"
#include <iostream>
#include <unordered_set>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAILED: " << message << '\n';
        return condition;
    }
}

int main()
{
    using namespace ChikaEngine;

    Reflection::InitAllReflection();
    Core::UIDGenerator::Instance().Init(19);

    Framework::Scene scene;
    const Core::GameObjectID parentId = scene.CreateGameobject("Parent");
    Framework::GameObject* parent = scene.GetGameObject(parentId);
    if (!Check(parent && parent->transform, "parent should exist with Transform"))
        return 1;

    const size_t initialObjectCount = scene.GetAllGameobjects().size();
    const Editor::CreateChildResult invalid = Editor::CommitCreateChild(scene, Core::InvalidGameObjectID);
    if (!Check(invalid.status == Editor::CreateChildStatus::InvalidParent, "invalid parent should be rejected")
        || !Check(scene.GetAllGameobjects().size() == initialObjectCount, "invalid request must not create an orphan"))
        return 1;

    std::unordered_set<Core::GameObjectID> childIds;
    constexpr size_t kChildCount = 50;
    for (size_t index = 0; index < kChildCount; ++index)
    {
        const Editor::CreateChildResult result = Editor::CommitCreateChild(scene, parentId);
        if (!Check(result.Succeeded(), "valid request should create a child")
            || !Check(Core::IsValidGameObjectID(result.childId), "created child should have a valid ID")
            || !Check(childIds.insert(result.childId).second, "created child IDs should be unique"))
            return 1;

        Framework::GameObject* child = scene.GetGameObject(result.childId);
        if (!Check(child && child->transform, "created child should resolve with Transform")
            || !Check(child->transform->GetParent() == parent->transform, "child should reference the requested parent")
            || !Check(child->transform->GetParentId() == parentId, "child serialized parent ID should match"))
            return 1;
    }

    if (!Check(parent->transform->GetChildren().size() == kChildCount, "parent should contain every created child")
        || !Check(scene.GetAllGameobjects().size() == initialObjectCount + kChildCount, "scene should contain parent and all children"))
        return 1;

    if (!Check(scene.StartPlayMode(), "scene should enter Play Mode for edit guard validation"))
        return 1;
    const size_t playObjectCount = scene.GetAllGameobjects().size();
    const Editor::CreateChildResult playing = Editor::CommitCreateChild(scene, parentId);
    if (!Check(playing.status == Editor::CreateChildStatus::SceneNotEditable, "Play Mode should reject hierarchy editing")
        || !Check(scene.GetAllGameobjects().size() == playObjectCount, "Play Mode rejection must not mutate the scene"))
        return 1;

    return 0;
}
