#include "include/ChikaEngine/ImguiAdapter.h"
#include "ChikaEngine/TimeSystem.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/debug/log_macros.h"
#include "IEditorPanel.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>
#include <string>

namespace ChikaEngine::Editor
{
    ImGuiAdapter::ImGuiAdapter(void* nativeWindow) : _window(nativeWindow) {}
    ImGuiAdapter::~ImGuiAdapter() = default;
    Core::GameObjectID id = 0;
    bool ImGuiAdapter::Init(void* nativeWindow)
    {
        GLFWwindow* window = static_cast<GLFWwindow*>(nativeWindow);
        if (!window)
        {
            LOG_ERROR("Editor", "ImGuiAdapter::Init failed: nativeWindow is null\n");
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        _window = nativeWindow;

        return true;
    }

    void ImGuiAdapter::NewFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiAdapter::RegisterPanel(IEditorPanel* panel)
    {
        if (!panel)
            return;
        _panels.push_back(panel);
    }

    void ImGuiAdapter::UnregisterPanel(IEditorPanel* panel)
    {
        _panels.erase(std::remove(_panels.begin(), _panels.end(), panel), _panels.end());
    }

    void ImGuiAdapter::RenderAllPanels()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("DockSpaceHost", nullptr, host_flags);
        ImGui::PopStyleVar();
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        // Setup default dock layout on first frame
        static bool initialized = false;
        if (!initialized)
        {
            // ImGui::DockBuilderFinish(dockspace_id);
            initialized = true;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

            // 重新分配位置，把 Content Browser 放下面，跟 Log Panel 一起
            ImGui::DockBuilderDockWindow("Scene View", dock_main_id);
            ImGui::DockBuilderDockWindow("Log Panel", dock_bottom_id);
            ImGui::DockBuilderDockWindow("Content Browser", dock_bottom_id); // <--- 加入这里
            ImGui::DockBuilderDockWindow("Inspector", dock_right_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        for (auto p : _panels)
            if (p)
                p->OnRender(_ctx);
        ImGui::End();
    }

    void ImGuiAdapter::Render()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiAdapter::SwapBuffers()
    {
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            auto backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }
    }

    void ImGuiAdapter::UpdateContext()
    {
        _ctx.deltaTime = Time::TimeSystem::GetDeltaTime();
        // _ctx.selection = {.fullName = "::ChikaEngine::Framework::Transform", .objectPtr = Framework::Scene::Instance().GetGOByID(id)->transform};
    }

    void ImGuiAdapter::Shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiAdapter::SaveLayout(const std::string& path)
    {
        ImGui::SaveIniSettingsToDisk(path.c_str());
    }
    void ImGuiAdapter::LoadLayout(const std::string& path)
    {
        ImGui::LoadIniSettingsFromDisk(path.c_str());
    }

    UIContext& ImGuiAdapter::GetContext()
    {
        return _ctx;
    }
} // namespace ChikaEngine::Editor