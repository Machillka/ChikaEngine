#include "InspectorPanel.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include <imgui.h>

namespace ChikaEngine::Editor
{
    void DrawReflectedObject(void* instance, const Reflection::ClassInfo* classInfo);

    void DrawProperty(const Reflection::PropertyInfo& prop, void* instance)
    {
        if (!prop.Get || !prop.Set)
            return; // 只读/未导出则跳过

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
            return;
        }

        switch (prop.Type)
        {
        case Reflection::ReflectType::Int:
        {
            int val;
            prop.Get(instance, &val);
            if (ImGui::DragInt(prop.Name.c_str(), &val))
                prop.Set(instance, &val);
            break;
        }
        case Reflection::ReflectType::Float:
        {
            float val;
            prop.Get(instance, &val);
            if (ImGui::DragFloat(prop.Name.c_str(), &val, 0.1f))
                prop.Set(instance, &val);
            break;
        }
        case Reflection::ReflectType::Bool:
        {
            bool val;
            prop.Get(instance, &val);
            if (ImGui::Checkbox(prop.Name.c_str(), &val))
                prop.Set(instance, &val);
            break;
        }
        case Reflection::ReflectType::String:
        {
            std::string val;
            prop.Get(instance, &val);
            char buffer[256];
            strncpy(buffer, val.c_str(), sizeof(buffer));
            if (ImGui::InputText(prop.Name.c_str(), buffer, sizeof(buffer)))
            {
                val = buffer;
                prop.Set(instance, &val);
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
                    prop.Set(instance, &val);
            }
            else if (prop.TypeName.find("Vector4") != std::string::npos)
            {
                Math::Vector4 val;
                prop.Get(instance, &val);
                if (ImGui::DragFloat4(prop.Name.c_str(), &val.x, 0.1f))
                    prop.Set(instance, &val);
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
    }

    void DrawReflectedObject(void* instance, const Reflection::ClassInfo* classInfo)
    {
        if (!classInfo || !instance)
            return;
        for (const auto& prop : classInfo->Properties)
        {
            DrawProperty(prop, instance);
        }
    }

    void InspectorPanel::OnImGuiRender()
    {
        ImGui::Begin(GetName().c_str(), &_isActive);

        if (_context->selectedGameObject != 0 && _context->activeScene)
        {
            auto* go = _context->activeScene->GetGameObject(_context->selectedGameObject);
            if (go)
            {
                // 1. GameObject 基础属性
                char nameBuf[256];
                strncpy(nameBuf, go->GetName().c_str(), sizeof(nameBuf));
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                    go->SetName(nameBuf);

                ImGui::TextDisabled("UID: %llu", go->GetID());

                bool active = go->IsActive();
                if (ImGui::Checkbox("Active", &active))
                    go->SetActive(active);
                ImGui::Separator();

                const auto* goClassInfo = Reflection::TypeRegister::Instance().GetClassByName("GameObject");
                if (goClassInfo)
                {
                    DrawReflectedObject(go, goClassInfo);
                }

                const auto& components = go->GetAllComponents();
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
                            DrawReflectedObject(comp.get(), compClassInfo);
                            ImGui::PopID();
                        }
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Add Component", ImVec2(-1, 0)))
                    ImGui::OpenPopup("AddComponentPopup");
                if (ImGui::BeginPopup("AddComponentPopup"))
                {
                    // TODO: 实现查找可反射类型允许添加
                    ImGui::TextDisabled("Component List...");
                    ImGui::EndPopup();
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