#include "VulkanAdapter.hpp"
#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/rhi/Vulkan/VulkanCommandList.hpp"
#include "ChikaEngine/rhi/Vulkan/VulkanRHIDevice.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <array>
#include <stdexcept>

namespace ChikaEngine::Editor
{
    void VulkanAdapter::Initialize(GLFWwindow* window, Render::Renderer* renderer)
    {
        Shutdown();

        if (!window || !renderer || !renderer->GetRHIHandle())
            throw std::invalid_argument("VulkanAdapter requires a window and initialized renderer");

        _renderer = renderer;
        _device = dynamic_cast<Render::VulkanRHIDevice*>(renderer->GetRHIHandle());
        if (!_device)
            throw std::runtime_error("VulkanAdapter requires the Vulkan RHI backend");

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

        const std::array<VkDescriptorPoolSize, 11> poolSizes{
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },          VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },          VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },         VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
        };
        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        if (vkCreateDescriptorPool(_device->GetRawDevice(), &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Editor UI descriptor pool");

        const VkFormat colorFormat = _device->GetSwapchainFormat();
        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_3;
        initInfo.Instance = _device->GetRawInstance();
        initInfo.PhysicalDevice = _device->GetRawPhysicalDevice();
        initInfo.Device = _device->GetRawDevice();
        initInfo.QueueFamily = _device->GetGraphicsQueueFamily();
        initInfo.Queue = _device->GetGraphicsQueue();
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = _device->GetSwapchainImageCount();
        initInfo.DescriptorPool = _descriptorPool;
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {};
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
        ImGui_ImplVulkan_Init(&initInfo);
        _vulkanBackendInitialized = true;
        _renderer->SetOverlayPassCallback([this](Render::IRHICommandList* commandList) { RenderOverlay(commandList); });
    }

    void VulkanAdapter::Shutdown()
    {
        if (_renderer)
            _renderer->SetOverlayPassCallback({});

        if (_vulkanBackendInitialized)
        {
            if (_device)
                _device->WaitIdle();

            ReleaseTextureDescriptor();
            ImGui_ImplVulkan_Shutdown();
            _vulkanBackendInitialized = false;
        }
        if (_descriptorPool != VK_NULL_HANDLE && _device)
        {
            vkDestroyDescriptorPool(_device->GetRawDevice(), _descriptorPool, nullptr);
            _descriptorPool = VK_NULL_HANDLE;
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
        _device = nullptr;
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
    }

    void* VulkanAdapter::GetTextureHandle(Render::TextureHandle texture)
    {
        if (!_device || !texture.IsValid())
            return nullptr;
        if (texture == _registeredTexture && _registeredTextureDescriptor != VK_NULL_HANDLE)
            return static_cast<void*>(_registeredTextureDescriptor);

        if (_registeredTextureDescriptor != VK_NULL_HANDLE)
        {
            _device->WaitIdle();
            ReleaseTextureDescriptor();
        }

        const Render::VulkanTexture* vulkanTexture = _device->GetVkTexture(texture);
        if (!vulkanTexture || vulkanTexture->view == VK_NULL_HANDLE)
            return nullptr;

        _registeredTextureDescriptor = ImGui_ImplVulkan_AddTexture(_device->GetDefaultSampler(), vulkanTexture->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        _registeredTexture = texture;
        return static_cast<void*>(_registeredTextureDescriptor);
    }

    void VulkanAdapter::RenderOverlay(Render::IRHICommandList* commandList)
    {
        auto* vulkanCommandList = dynamic_cast<Render::VulkanCommandList*>(commandList);
        if (!vulkanCommandList)
            return;
        if (ImDrawData* drawData = ImGui::GetDrawData())
            ImGui_ImplVulkan_RenderDrawData(drawData, vulkanCommandList->GetVkCmdRaw());
    }

    void VulkanAdapter::ReleaseTextureDescriptor()
    {
        if (_registeredTextureDescriptor != VK_NULL_HANDLE)
            ImGui_ImplVulkan_RemoveTexture(_registeredTextureDescriptor);
        _registeredTextureDescriptor = VK_NULL_HANDLE;
        _registeredTexture = {};
    }
} // namespace ChikaEngine::Editor
