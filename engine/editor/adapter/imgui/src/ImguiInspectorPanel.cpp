
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
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

        // TODO: 实现鼠标点击选中物体并且展示gizmos
        auto selectionObj = ctx.selection;
        if (!selectionObj.objectPtr)
        {
            ImGui::TextUnformatted("Inspector:\nSelect an object to see properties.");
            ImGui::End();
            return;
        }

        const auto* ci = Reflection::TypeRegister::Instance().GetClass(selectionObj.fullName);
        if (!ci)
        {
            ImGui::Text("No reflection info for %s", selectionObj.fullName.c_str());
            ImGui::End();
            return;
        }
        ImGui::Text("%s", ci->Name.c_str());
        ImGui::Separator();
        // 遍历属性
        if (ImGui::BeginTable("InspectorTable", 2, ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            for (const auto& prop : ci->Properties)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                // 属性名显示
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(prop.Name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID(prop.Name.c_str());
                DrawPropertyWidget(selectionObj.objectPtr, prop);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }

    void ImguiInspectorPanel::DrawPropertyWidget(void* instance, const Reflection::PropertyInfo& prop)
    {
        using ChikaEngine::Reflection::ReflectType;
        switch (prop.Type)
        {
        case ReflectType::Float:
        {
            float value = 0.0f;
            prop.Get(instance, &value);
            if (ImGui::DragFloat("##val", &value, 0.01f))
            {
                prop.Set(instance, &value);
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
            }
            q = q.Normalized();
            prop.Set(instance, &q);
            break;
        }
        case ReflectType::Unknown:
        default:
            ImGui::TextUnformatted("<unknown>");
        }
    }

} // namespace ChikaEngine::Editor
