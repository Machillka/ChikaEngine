#include "HierarchyActions.hpp"

#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"
#include <string>

namespace ChikaEngine::Editor
{
    CreateChildResult CommitCreateChild(Framework::Scene& scene, Core::GameObjectID parentId, std::string_view childName)
    {
        if (!scene.IsEditing())
            return { .status = CreateChildStatus::SceneNotEditable };

        Framework::GameObject* parent = scene.GetGameObject(parentId);
        if (!parent || !parent->transform)
            return { .status = CreateChildStatus::InvalidParent };

        const Core::GameObjectID childId = scene.CreateGameobject(std::string(childName));
        if (!Core::IsValidGameObjectID(childId))
            return { .status = CreateChildStatus::CreateFailed };

        Framework::GameObject* child = scene.GetGameObject(childId);
        if (!child || !child->transform || !child->transform->SetParent(parent->transform, true))
        {
            scene.DestroyGameObject(childId);
            return { .status = CreateChildStatus::ParentRejected };
        }

        return {
            .status = CreateChildStatus::Success,
            .childId = childId,
        };
    }

    const char* CreateChildStatusName(CreateChildStatus status) noexcept
    {
        switch (status)
        {
        case CreateChildStatus::Success:
            return "Success";
        case CreateChildStatus::SceneNotEditable:
            return "SceneNotEditable";
        case CreateChildStatus::InvalidParent:
            return "InvalidParent";
        case CreateChildStatus::CreateFailed:
            return "CreateFailed";
        case CreateChildStatus::ParentRejected:
            return "ParentRejected";
        }
        return "Unknown";
    }
} // namespace ChikaEngine::Editor
