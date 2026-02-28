
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "imgui.h"
#include "include/ChikaEngine/ImguiInspectorPanel.h"
#include "include/ChikaEngine/ImguiInspectorHelper.h"

#include <cstdlib>

namespace ChikaEngine::Editor
{
    void ImguiInspectorPanel::OnRender(UIContext& ctx)
    {
        ImGui::Begin(Name());
        // TODO: Reflect selected object's properties here

        // TODO: 实现鼠标点击选中物体并且展示 gizmos
        auto selectionObj = ctx.selection;
        if (!selectionObj.objectPtr)
        {
            ImGui::TextUnformatted("Inspector:\nSelect an object to see properties.");
            ImGui::End();
            return;
        }

        const auto* ci = Reflection::TypeRegister::Instance().GetClassByName(selectionObj.fullName);
        if (!ci)
        {
            ImGui::Text("No reflection info for %s", selectionObj.fullName.c_str());
            ImGui::End();
            return;
        }

        if (selectionObj.fullName == "GameObject")
        {
            auto* go = static_cast<Framework::GameObject*>(selectionObj.objectPtr);

            // 绘制 GameObject 基本信息 (如 Name)
            if (ImGui::CollapsingHeader("GameObject", ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawReflectedProperties(go, "GameObject");
            }
            // 遍历并绘制所有组件
            const auto& components = go->GetAllComponents();
            for (const auto& compPtr : components)
            {
                Framework::Component* comp = compPtr.get();
                if (!comp)
                    continue;

                std::string compClassName = comp->GetReflectedClassName();

                if (ImGui::CollapsingHeader(compClassName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    DrawReflectedProperties(comp, compClassName);
                }
            }
        }
        else
        {
            // 2. 如果选中的是普通反射对象，直接按原逻辑绘制
            if (ImGui::CollapsingHeader(selectionObj.fullName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawReflectedProperties(selectionObj.objectPtr, selectionObj.fullName);
            }
        }
        ImGui::End();
    }

    bool ImguiInspectorPanel::DrawPropertyWidget(void* instance, const Reflection::PropertyInfo& prop)
    {
        using ChikaEngine::Reflection::ReflectType;
        bool isChanged = false;
        switch (prop.Type)
        {
        case ReflectType::Float:
        {
            float value = 0.0f;
            prop.Get(instance, &value);
            if (ImGui::DragFloat("##val", &value, 0.01f))
            {
                prop.Set(instance, &value);
                isChanged = true;
            }
            break;
        }
        case ReflectType::Int:
        {
            int v = 0;
            prop.Get(instance, &v);
            if (ImGui::InputInt("##val", &v))
            {
                prop.Set(instance, &v);
                isChanged = true;
            }
            break;
        }
        case ReflectType::Bool:
        {
            bool b = false;
            prop.Get(instance, &b);
            if (ImGui::Checkbox("##val", &b))
            {
                prop.Set(instance, &b);
                isChanged = true;
            }
            break;
        }
        case ReflectType::Vector3:
        {
            Math::Vector3 v;
            prop.Get(instance, &v);
            if (ImGuiHelpers::DrawVec3Control("", v))
            {
                prop.Set(instance, &v);
                isChanged = true;
            }
            break;
        }
        case ReflectType::Quaternion:
        {
            Math::Quaternion q;
            prop.Get(instance, &q);
            float vals[4] = {q.x, q.y, q.z, q.w};
            if (ImGui::InputFloat4("##quat", vals))
            {
                q.x = vals[0];
                q.y = vals[1];
                q.z = vals[2];
                q.w = vals[3];
                q = q.Normalized();
                prop.Set(instance, &q);
                isChanged = true;
            }

            break;
        }
        case ReflectType::String:
        {
            std::string s;
            prop.Get(instance, &s);

            // ImGui 需要一个 char 缓冲区
            char buf[256];
            memset(buf, 0, sizeof(buf));
            strncpy(buf, s.c_str(), sizeof(buf) - 1);

            if (ImGui::InputText("##val", buf, sizeof(buf)))
            {
                s = std::string(buf);
                prop.Set(instance, &s);
                isChanged = true;
            }
            break;
        }
        case ReflectType::Unknown:
        default:
            ImGui::TextUnformatted("<unknown>");
        }

        return isChanged;
    }

    void ImguiInspectorPanel::DrawReflectedProperties(void* instance, const std::string& typeName)
    {
        const auto* ci = Reflection::TypeRegister::Instance().GetClassByName(typeName);
        if (!ci)
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No reflection info for %s", typeName.c_str());
            return;
        }
        bool anyChanged = false;
        if (ImGui::BeginTable((typeName + "_Table").c_str(), 2, ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            for (const auto& prop : ci->Properties)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(prop.Name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID(prop.Name.c_str());
                if (DrawPropertyWidget(instance, prop))
                {
                    anyChanged = true;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (anyChanged)
        {
            if (typeName != "GameObject")
            {
                auto* comp = static_cast<Framework::Component*>(instance);
                comp->OnPropertyChanged();
            }
        }
    }

} // namespace ChikaEngine::Editor
