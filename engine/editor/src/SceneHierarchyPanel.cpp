#include "SceneHierarchyPanel.hpp"
#include "HierarchyActions.hpp"
#include "ChikaEngine/debug/log_macros.h"
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
        const Core::GameObjectID gameObjectId = gameObject.GetID();
        const bool expandThisFrame = _expandOnNextDraw == gameObjectId;
        if (expandThisFrame)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!gameObject.transform || gameObject.transform->GetChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (_context->selectedGameObject == gameObject.GetID())
            flags |= ImGuiTreeNodeFlags_Selected;

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(gameObjectId), flags, "%s", gameObject.GetName().c_str());
        if (expandThisFrame)
            _expandOnNextDraw = Core::InvalidGameObjectID;
        if (ImGui::IsItemClicked())
            _context->selectedGameObject = gameObjectId;

        if (ImGui::BeginPopupContextItem())
        {
            _context->selectedGameObject = gameObjectId;

            if (_context->activeScene->IsEditing())
            {
                if (ImGui::MenuItem("Create Child"))
                    _pendingCreateChildParent = gameObjectId;
                DrawAddComponentMenu(gameObject, _context->isDirty);
            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::MenuItem("Create Child");
                ImGui::MenuItem("Add Component");
                ImGui::EndDisabled();
                ImGui::Separator();
                ImGui::TextDisabled("Return to Edit Mode to modify hierarchy.");
            }
            ImGui::EndPopup();
        }

        if (_context->activeScene->IsEditing())
        {
            if (ImGui::BeginDragDropSource())
            {
                const auto id = gameObjectId;
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

    void SceneHierarchyPanel::CommitPendingCreateChild()
    {
        if (!_pendingCreateChildParent)
            return;

        const Core::GameObjectID parentId = *_pendingCreateChildParent;
        _pendingCreateChildParent.reset();

        Framework::Scene* scene = _context ? _context->activeScene : nullptr;
        if (!scene)
            return;

        const CreateChildResult result = CommitCreateChild(*scene, parentId);
        if (!result.Succeeded())
        {
            LOG_ERROR("Editor", "Create Child failed for parent {}: {}", parentId, CreateChildStatusName(result.status));
            return;
        }

        _context->selectedGameObject = result.childId;
        _context->isDirty = true;
        _expandOnNextDraw = parentId;
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

        if (Core::IsValidGameObjectID(_expandOnNextDraw) && !scene->GetGameObject(_expandOnNextDraw))
            _expandOnNextDraw = Core::InvalidGameObjectID;

        const bool editing = scene->IsEditing();
        if (!editing)
            ImGui::BeginDisabled();
        const bool createRoot = ImGui::Button("Create GameObject");
        if (!editing)
        {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("Edit Mode only");
        }

        if (editing && createRoot)
        {
            const Core::GameObjectID createdId = scene->CreateGameobject("GameObject");
            if (Core::IsValidGameObjectID(createdId))
            {
                _context->selectedGameObject = createdId;
                _context->isDirty = true;
            }
            else
            {
                LOG_ERROR("Editor", "Create GameObject failed");
            }
        }

        if (editing && ImGui::BeginDragDropTarget())
        {
            if (const auto* payload = ImGui::AcceptDragDropPayload("CHIKA_GAME_OBJECT"))
            {
                const auto id = *static_cast<const Core::GameObjectID*>(payload->Data);
                if (auto* object = scene->GetGameObject(id); object && object->transform)
                    object->transform->SetParent(nullptr, true);
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextWindow("SceneHierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (editing)
            {
                if (ImGui::MenuItem("Create GameObject"))
                {
                    const Core::GameObjectID createdId = scene->CreateGameobject("GameObject");
                    if (Core::IsValidGameObjectID(createdId))
                    {
                        _context->selectedGameObject = createdId;
                        _context->isDirty = true;
                    }
                    else
                    {
                        LOG_ERROR("Editor", "Create GameObject failed");
                    }
                }
            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::MenuItem("Create GameObject");
                ImGui::EndDisabled();
                ImGui::Separator();
                ImGui::TextDisabled("Return to Edit Mode to modify hierarchy.");
            }
            ImGui::EndPopup();
        }

        for (const auto& object : scene->GetAllGameobjects())
        {
            if (!object->transform || !object->transform->GetParent())
                DrawGameObjectNode(*object);
        }

        // Scene structural mutations are committed only after traversal so vector iterators remain valid.
        CommitPendingCreateChild();

        ImGui::End();
    }
} // namespace ChikaEngine::Editor
