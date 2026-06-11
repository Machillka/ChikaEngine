#include "SceneHierarchyPanel.hpp"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"
#include <imgui.h>

namespace ChikaEngine::Editor
{
    void SceneHierarchyPanel::DrawGameObjectNode(Framework::GameObject& gameObject)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!gameObject.transform || gameObject.transform->GetChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (_context->selectedGameObject == gameObject.GetID())
            flags |= ImGuiTreeNodeFlags_Selected;

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(gameObject.GetID()), flags, "%s", gameObject.GetName().c_str());
        if (ImGui::IsItemClicked())
            _context->selectedGameObject = gameObject.GetID();

        if (_context->activeScene->IsEditing())
        {
            if (ImGui::BeginDragDropSource())
            {
                const auto id = gameObject.GetID();
                ImGui::SetDragDropPayload("CHIKA_GAME_OBJECT", &id, sizeof(id));
                ImGui::TextUnformatted(gameObject.GetName().c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const auto* payload = ImGui::AcceptDragDropPayload("CHIKA_GAME_OBJECT"))
                {
                    const auto childId = *static_cast<const Core::GameObjectID*>(payload->Data);
                    if (auto* child = _context->activeScene->GetGameObject(childId); child && child->transform && gameObject.transform)
                        child->transform->SetParent(gameObject.transform, true);
                }
                ImGui::EndDragDropTarget();
            }
        }

        if (open)
        {
            if (gameObject.transform)
            {
                for (auto* child : gameObject.transform->GetChildren())
                {
                    if (child && child->GetOwner())
                        DrawGameObjectNode(*child->GetOwner());
                }
            }
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin(GetName().c_str(), &_isActive);

        auto* scene = _context->activeScene;
        if (!scene)
        {
            ImGui::TextDisabled("No active scene");
            ImGui::End();
            return;
        }

        if (scene->IsEditing() && ImGui::Button("Create GameObject"))
        {
            _context->selectedGameObject = scene->CreateGameobject("GameObject");
            _context->isDirty = true;
        }

        if (scene->IsEditing() && ImGui::BeginDragDropTarget())
        {
            if (const auto* payload = ImGui::AcceptDragDropPayload("CHIKA_GAME_OBJECT"))
            {
                const auto id = *static_cast<const Core::GameObjectID*>(payload->Data);
                if (auto* object = scene->GetGameObject(id); object && object->transform)
                    object->transform->SetParent(nullptr, true);
            }
            ImGui::EndDragDropTarget();
        }

        for (const auto& object : scene->GetAllGameobjects())
        {
            if (!object->transform || !object->transform->GetParent())
                DrawGameObjectNode(*object);
        }

        ImGui::End();
    }
} // namespace ChikaEngine::Editor
