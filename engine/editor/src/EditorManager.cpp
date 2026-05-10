#include <imgui.h>
#include <imgui_internal.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "EditorManager.hpp"
#include "ViewportPanel.hpp"
#include "imgui_impl_glfw.h"

namespace ChikaEngine::Editor
{
    void EditorManager::Initialize(const EditorCreateInfo& createInfo)
    {
        _renderer = createInfo.renderer;
        _context.renderer = createInfo.renderer;
        _windowHandle = static_cast<GLFWwindow*>(createInfo.window);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui_ImplGlfw_InitForVulkan(_windowHandle, true);
        _renderer->GetRHIHandle()->InitializeImgui();
        // TODO: 添加字体上传逻辑
        AddPanel<ViewportPanel>();
    }

    void EditorManager::Tick(float deltaTime)
    {
        for (auto& panel : _panels)
        {
            panel->Tick(deltaTime);
        }
        OnImGuiRender();
        _renderer->SubmitImGuiData(ImGui::GetDrawData());
        ImGui::Render();
    }

    void EditorManager::OnImGuiRender()
    {
        for (auto& panel : _panels)
        {
            panel->OnImGuiRender();
        }
    }

    void EditorManager::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
    void EditorManager::EndFrame() {}

} // namespace ChikaEngine::Editor