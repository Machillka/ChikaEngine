#include "ImguiAdapter.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "debug/log_macros.h"
#include "imgui.h"

#include <GLFW/glfw3.h>
#include <string>

namespace ChikaEngine::Editor
{
    ImGuiAdapter::ImGuiAdapter(void* nativeWindow) : _window(nativeWindow) {}
    ImGuiAdapter::~ImGuiAdapter() = default;

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
        io.ConfigFlags |= ImGuiDockNodeFlags_None;
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
        UIContext ctx;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("DockSpaceHost", nullptr, host_flags);
        ImGui::PopStyleVar();
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        for (auto p : _panels)
            if (p)
                p->OnRender(ctx);
        ImGui::End();
    }

    void ImGuiAdapter::Render()
    {
        LOG_INFO("Editor", "ImGui Rendering");
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

} // namespace ChikaEngine::Editor