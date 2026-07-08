#include "SceneHierarchyPanel.hpp"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/LightComponent.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/component/ScriptComponent.h"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"
#include <imgui.h>

namespace ChikaEngine::Editor
{
    namespace
    {
        void DrawAddComponentMenu(Framework::GameObject& gameObject, bool& isDirty)
        {
            if (!ImGui::BeginMenu("Add Component"))
                return;

            if (ImGui::MenuItem("MeshRenderer"))
            {
                gameObject.AddComponent<Framework::MeshRenderer>();
                isDirty = true;
            }
            if (ImGui::MenuItem("Light"))
            {
                gameObject.AddComponent<Framework::LightComponent>();
                isDirty = true;
            }
            if (ImGui::MenuItem("Animator"))
            {
                gameObject.AddComponent<Framework::Animator>();
                isDirty = true;
            }
            if (ImGui::MenuItem("Rigidbody"))
            {
                gameObject.AddComponent<Framework::Rigidbody>();
                isDirty = true;
            }
            if (ImGui::MenuItem("ScriptComponent"))
            {
                gameObject.AddComponent<Framework::ScriptComponent>();
                isDirty = true;
            }

            ImGui::EndMenu();
        }
    } // namespace

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
            if (ImGui::BeginPopupContextItem())
            {
                _context->selectedGameObject = gameObject.GetID();

                if (ImGui::MenuItem("Create Child"))
                {
                    const auto childId = _context->activeScene->CreateGameobject("GameObject");
                    if (auto* child = _context->activeScene->GetGameObject(childId); child && child->transform && gameObject.transform)
                        child->transform->SetParent(gameObject.transform, true);
                    _context->selectedGameObject = childId;
                    _context->isDirty = true;
                }
                DrawAddComponentMenu(gameObject, _context->isDirty);
                ImGui::EndPopup();
            }

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

        if (scene->IsEditing() && ImGui::BeginPopupContextWindow("SceneHierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create GameObject"))
            {
                _context->selectedGameObject = scene->CreateGameobject("GameObject");
                _context->isDirty = true;
            }
            ImGui::EndPopup();
        }

        for (const auto& object : scene->GetAllGameobjects())
        {
            if (!object->transform || !object->transform->GetParent())
                DrawGameObjectNode(*object);
        }

        ImGui::End();
    }
} // namespace ChikaEngine::Editor
