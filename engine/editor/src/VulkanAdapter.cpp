#include "VulkanAdapter.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/IRHIDevice.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h> // Editor Adapter 仅使用它来调用 NewFrame
#include <stdexcept>

namespace ChikaEngine::Editor
{
    void VulkanAdapter::Initialize(GLFWwindow* window, Render::Renderer* renderer)
    {
        Shutdown();

        if (!window || !renderer || !renderer->GetRHIHandle())
            throw std::invalid_argument("VulkanAdapter requires a window and initialized renderer");

        _renderer = renderer;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        _contextInitialized = true;
        ImGuiIO& io = ImGui::GetIO();
        (void)io;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForVulkan(window, true))
            throw std::runtime_error("Failed to initialize ImGui GLFW backend");
        _glfwBackendInitialized = true;

        _renderer->GetRHIHandle()->InitializeImgui();
        _vulkanBackendInitialized = true;
    }

    void VulkanAdapter::Shutdown()
    {
        if (_vulkanBackendInitialized)
        {
            if (_renderer && _renderer->GetRHIHandle())
                _renderer->GetRHIHandle()->WaitIdle();

            ImGui_ImplVulkan_Shutdown();
            _vulkanBackendInitialized = false;
        }
        if (_glfwBackendInitialized)
        {
            ImGui_ImplGlfw_Shutdown();
            _glfwBackendInitialized = false;
        }
        if (_contextInitialized)
        {
            ImGui::DestroyContext();
            _contextInitialized = false;
        }
        _renderer = nullptr;
    }

    void VulkanAdapter::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void VulkanAdapter::EndFrame()
    {
        ImGui::Render();

        _renderer->SubmitImGuiData(ImGui::GetDrawData());
    }
} // namespace ChikaEngine::Editor
