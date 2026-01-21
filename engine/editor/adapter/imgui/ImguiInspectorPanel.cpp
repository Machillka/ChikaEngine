#include "ImguiInspectorPanel.h"

#include "imgui.h"

namespace ChikaEngine::Editor
{
    void ImguiInspectorPanel::OnRender(UIContext& ctx)
    {
        ImGui::Begin(Name());
        ImGui::TextUnformatted("Inspector:\nSelect an object to see properties.");
        // TODO: Reflect selected object's properties here
        ImGui::End();
    }
} // namespace ChikaEngine::Editor
