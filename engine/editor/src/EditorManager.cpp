#include <imgui.h>
#include <imgui_internal.h>

#include "InspectorPanel.hpp"
#include "LogPanel.hpp"
#include "backends/imgui_impl_glfw.h"

#include "EditorManager.hpp"
#include "ViewportPanel.hpp"
#include "imgui_impl_glfw.h"

namespace ChikaEngine::Editor
{
    void EditorManager::Initialize(const EditorCreateInfo& createInfo)
    {
        _renderer = createInfo.renderer;
        _windowHandle = static_cast<GLFWwindow*>(createInfo.window);
        _context.activeScene = createInfo.scene;
        _context.renderer = _renderer;

        _adapter.Initialize(_windowHandle, _renderer);

        AddPanel<ViewportPanel>();
        AddPanel<InspectorPanel>();
        AddPanel<LogPanel>();
    }

    void EditorManager::Shutdown()
    {
        _panels.clear();
        _adapter.Shutdown();
    }

    void EditorManager::Tick(float deltaTime)
    {
        for (auto& panel : _panels)
        {
            if (panel->IsActive())
                panel->Tick(deltaTime);
        }
    }

    void EditorManager::BeginFrame()
    {
        _adapter.BeginFrame();
    }
    void EditorManager::EndFrame()
    {
        _adapter.EndFrame();
    }

    void EditorManager::BeginDockspace()
    {
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("ChikaEngine DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        static bool first_time = true;
        if (first_time)
        {
            first_time = false;
            ImGui::DockBuilderRemoveNode(dockspace_id); // 清除之前可能遗留的布局
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_main_id = dockspace_id;

            // 1. 向右切分出 25% 宽度给 Inspector
            ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            // 2. 向左切分出 20% 宽度给左侧功能区
            ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            // 3. 将左侧功能区上下平分给 Scene 和 File
            ImGuiID dock_left_top_id = ImGui::DockBuilderSplitNode(dock_left_id, ImGuiDir_Up, 0.50f, nullptr, &dock_left_id);
            ImGuiID dock_left_bottom_id = dock_left_id;

            // 绑定面板名字到对应区域 (注意：这里的字符串必须与面板的 GetName() 完全一致)
            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
            ImGui::DockBuilderDockWindow("Inspector", dock_right_id);
            ImGui::DockBuilderDockWindow("Scene", dock_left_top_id);
            ImGui::DockBuilderDockWindow("File", dock_left_bottom_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    void EditorManager::EndDockspace()
    {
        ImGui::End();
    }

    void EditorManager::OnImGuiRender()
    {
        BeginDockspace();

        for (auto& panel : _panels)
        {
            if (panel->IsActive())
            {
                panel->OnImGuiRender();
            }
        }
        EndDockspace();
    }

} // namespace ChikaEngine::Editor