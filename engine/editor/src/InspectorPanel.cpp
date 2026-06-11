#include "InspectorPanel.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/component/ScriptComponent.h"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include <imgui.h>

namespace ChikaEngine::Editor
{
    bool DrawReflectedObject(void* instance, const Reflection::ClassInfo* classInfo);

    bool DrawProperty(const Reflection::PropertyInfo& prop, void* instance)
    {
        if (!prop.Get || !prop.Set)
            return false;

        if (prop.IsPointer)
        {
            void* ptrValue = nullptr;
            prop.Get(instance, &ptrValue); // 取出实际指针
            if (ptrValue)
            {
                std::string typeName = prop.TypeName;
                if (typeName.back() == '*')
                    typeName.pop_back();

                const auto* refClass = Reflection::TypeRegister::Instance().GetClassByName(typeName);
                if (refClass)
                {
                    if (ImGui::TreeNodeEx(prop.Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        DrawReflectedObject(ptrValue, refClass);
                        ImGui::TreePop();
                    }
                }
                else
                {
                    ImGui::TextDisabled("Unregistered Pointer: %s", prop.Name.c_str());
                }
            }
            return false;
        }

        switch (prop.Type)
        {
        case Reflection::ReflectType::Int:
        {
            int val;
            prop.Get(instance, &val);
            if (ImGui::DragInt(prop.Name.c_str(), &val))
            {
                prop.Set(instance, &val);
                return true;
            }
            break;
        }
        case Reflection::ReflectType::Float:
        {
            float val;
            prop.Get(instance, &val);
            if (ImGui::DragFloat(prop.Name.c_str(), &val, 0.1f))
            {
                prop.Set(instance, &val);
                return true;
            }
            break;
        }
        case Reflection::ReflectType::Bool:
        {
            bool val;
            prop.Get(instance, &val);
            if (ImGui::Checkbox(prop.Name.c_str(), &val))
            {
                prop.Set(instance, &val);
                return true;
            }
            break;
        }
        case Reflection::ReflectType::String:
        {
            std::string val;
            prop.Get(instance, &val);
            char buffer[256];
            strncpy_s(buffer, val.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText(prop.Name.c_str(), buffer, sizeof(buffer)))
            {
                val = buffer;
                prop.Set(instance, &val);
                return true;
            }
            break;
        }
        case Reflection::ReflectType::Object:
        {
            if (prop.TypeName.find("Vector3") != std::string::npos)
            {
                Math::Vector3 val;
                prop.Get(instance, &val);
                if (ImGui::DragFloat3(prop.Name.c_str(), &val.x, 0.1f))
                {
                    prop.Set(instance, &val);
                    return true;
                }
            }
            else if (prop.TypeName.find("Vector4") != std::string::npos)
            {
                Math::Vector4 val;
                prop.Get(instance, &val);
                if (ImGui::DragFloat4(prop.Name.c_str(), &val.x, 0.1f))
                {
                    prop.Set(instance, &val);
                    return true;
                }
            }
            else
            {
                ImGui::TextDisabled("Unsupported ByValue Object Edit: %s", prop.Name.c_str());
            }
            break;
        }
        default:
            ImGui::TextDisabled("Unsupported Type: %s", prop.Name.c_str());
            break;
        }
        return false;
    }

    bool DrawReflectedObject(void* instance, const Reflection::ClassInfo* classInfo)
    {
        if (!classInfo || !instance)
            return false;
        bool changed = false;
        for (const auto& prop : classInfo->Properties)
        {
            changed = DrawProperty(prop, instance) || changed;
        }
        return changed;
    }

    void InspectorPanel::OnImGuiRender()
    {
        ImGui::Begin(GetName().c_str(), &_isActive);

        if (_context->selectedGameObject != 0 && _context->activeScene)
        {
            auto* scene = _context->activeScene;
            auto* go = scene->GetGameObject(_context->selectedGameObject);
            if (go)
            {
                // 1. GameObject 基础属性
                char nameBuf[256];
                strncpy_s(nameBuf, go->GetName().c_str(), sizeof(nameBuf) - 1);
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                {
                    go->SetName(nameBuf);
                    _context->isDirty = scene->IsEditing();
                }

                ImGui::TextDisabled("UID: %llu", go->GetID());

                bool active = go->IsActive();
                if (ImGui::Checkbox("Active", &active))
                {
                    go->SetActive(active);
                    _context->isDirty = scene->IsEditing();
                }
                ImGui::Separator();

                const auto* goClassInfo = Reflection::TypeRegister::Instance().GetClassByName("GameObject");
                if (goClassInfo)
                {
                    if (DrawReflectedObject(go, goClassInfo))
                        _context->isDirty = scene->IsEditing();
                }

                const auto& components = go->GetAllComponents();
                Framework::Component* componentToRemove = nullptr;
                for (const auto& comp : components)
                {
                    std::string compTypeName = comp->GetReflectedClassName();
                    const auto* compClassInfo = Reflection::TypeRegister::Instance().GetClassByName(compTypeName);

                    if (compClassInfo)
                    {
                        auto shortName = compTypeName.substr(compTypeName.find_last_of(":") + 1);
                        if (ImGui::CollapsingHeader(shortName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::PushID(comp.get());
                            if (DrawReflectedObject(comp.get(), compClassInfo))
                            {
                                comp->MarkDirty();
                                _context->isDirty = scene->IsEditing();
                            }
                            if (comp.get() != go->transform && ImGui::Button("Remove Component"))
                                componentToRemove = comp.get();
                            ImGui::PopID();
                        }
                    }
                }
                if (componentToRemove)
                {
                    go->RemoveComponent(componentToRemove);
                    _context->isDirty = scene->IsEditing();
                }

                ImGui::Spacing();
                if (ImGui::Button("Add Component", ImVec2(-1, 0)))
                    ImGui::OpenPopup("AddComponentPopup");
                if (ImGui::BeginPopup("AddComponentPopup"))
                {
                    if (ImGui::MenuItem("MeshRenderer"))
                    {
                        go->AddComponent<Framework::MeshRenderer>();
                        _context->isDirty = scene->IsEditing();
                    }
                    if (ImGui::MenuItem("Animator"))
                    {
                        go->AddComponent<Framework::Animator>();
                        _context->isDirty = scene->IsEditing();
                    }
                    if (ImGui::MenuItem("Rigidbody"))
                    {
                        go->AddComponent<Framework::Rigidbody>();
                        _context->isDirty = scene->IsEditing();
                    }
                    if (ImGui::MenuItem("ScriptComponent"))
                    {
                        go->AddComponent<Framework::ScriptComponent>();
                        _context->isDirty = scene->IsEditing();
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::Button("Destroy GameObject", ImVec2(-1, 0)))
                {
                    scene->DestroyGameObject(go->GetID());
                    _context->selectedGameObject = Core::InvalidGameObjectID;
                    _context->isDirty = scene->IsEditing();
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No GameObject Selected");
        }

        ImGui::End();
    }
} // namespace ChikaEngine::Editor
