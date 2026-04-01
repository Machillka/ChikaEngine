#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/rhi/Vulkan/VulkanCommandList.hpp"
#include "ChikaEngine/rhi/Vulkan/VulkanResource.hpp"
#include <ChikaEngine/rhi/Vulkan/VulkanRHIDevice.hpp>
#include <ChikaEngine/rhi/Vulkan/VulkanHelper.hpp>
#include <array>
#include <backends/imgui_impl_vulkan.h>
#include <cstdint>
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <vector>

namespace ChikaEngine::Render
{
    VulkanRHIDevice::VulkanRHIDevice(const RHI_InitParams& params)
    {
        Initialize(params);
    }

    VulkanRHIDevice::~VulkanRHIDevice()
    {
        Shutdown();
    }

    void VulkanRHIDevice::Initialize(const RHI_InitParams& params)
    {
        // 先赋值参数 后期在 初始化 vulkan 的时候再做 check
        m_windowHandle = params.nativeWindowHandle;
        m_height = params.height;
        m_width = params.width;

        CreateInstance();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateAllocator();
        CreateGlobalLayouts();
        CreateCommandPools();
        CreateSwapchain();
        CreateSyncObjects();
    }
    void VulkanRHIDevice::Shutdown()
    {
        if (!m_device)
            return;
        vkDeviceWaitIdle(m_device);

        // for (auto imageView : m_swapchainImageViews)
        // {
        //     vkDestroyImageView(m_device, imageView, nullptr);
        // }
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

        for (uint32_t i = 0; i < m_imageCount; i++)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        }

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            // vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
            vkDestroyCommandPool(m_device, m_commandPools[i], nullptr);
            vkDestroyDescriptorPool(m_device, m_descriptorPools[i], nullptr);
        }

        vkDestroySampler(m_device, m_defaultSampler, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_globalDescriptorLayout, nullptr);

        m_buffers.ForEach([&](auto h, VulkanBuffer& vb) { vmaDestroyBuffer(m_allocator, vb.buffer, vb.allocation); });
        m_textures.ForEach(
            [&](auto h, VulkanTexture& tex)
            {
                if (tex.view)
                    vkDestroyImageView(m_device, tex.view, nullptr);
                if (tex.allocation)
                    vmaDestroyImage(m_allocator, tex.image, tex.allocation);
            });
        m_shaders.ForEach(
            [&](auto h, VulkanShader& vs)
            {
                if (vs.module)
                    vkDestroyShaderModule(m_device, vs.module, nullptr);
            });

        m_pipelines.ForEach(
            [&](auto h, VulkanPipeline& pso)
            {
                if (pso.pipeline)
                    vkDestroyPipeline(m_device, pso.pipeline, nullptr);
                if (pso.layout)
                    vkDestroyPipelineLayout(m_device, pso.layout, nullptr);
            });
        vmaDestroyAllocator(m_allocator);

        vkDestroyDevice(m_device, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);

        m_buffers.Clear();
        m_textures.Clear();
        m_shaders.Clear();
        m_pipelines.Clear();
    }

    void VulkanRHIDevice::BeginFrame()
    {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

        VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex), "Failed to acquire swapchain image");

        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

        vkResetCommandPool(m_device, m_commandPools[m_currentFrame], 0);
        vkResetDescriptorPool(m_device, m_descriptorPools[m_currentFrame], 0);

        m_pendingCmds[m_currentFrame].clear();
        FlushDeletionQueue();
    }

    void VulkanRHIDevice::EndFrame()
    {
        // Submit
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
        submitInfo.pWaitDstStageMask = waitStages;

        if (!m_pendingCmds[m_currentFrame].empty())
        {
            submitInfo.commandBufferCount = static_cast<uint32_t>(m_pendingCmds[m_currentFrame].size());
            submitInfo.pCommandBuffers = m_pendingCmds[m_currentFrame].data();
        }
        else
        {
            submitInfo.commandBufferCount = 0;
            submitInfo.pCommandBuffers = nullptr;
        }

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentImageIndex];

        VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]), "Failed to submit graphics queue");

        // Present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        if (m_pendingCmds[m_currentFrame].empty())
        {
            presentInfo.waitSemaphoreCount = 0;
            presentInfo.pWaitSemaphores = nullptr;
        }
        else
        {
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentImageIndex];
        }
        // presentInfo.waitSemaphoreCount = 1;
        // presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swapchain;
        presentInfo.pImageIndices = &m_currentImageIndex;

        vkQueuePresentKHR(m_graphicsQueue, &presentInfo);

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        m_absoluteFrame++;
    }

    void VulkanRHIDevice::Submit(IRHICommandList* cmdList)
    {
        VulkanCommandList* vCmd = static_cast<VulkanCommandList*>(cmdList);
        m_pendingCmds[m_currentFrame].push_back(vCmd->GetVkCmdRaw());
        // TODO: 写个 cmd 池
        // delete vCmd;
    }

    BufferHandle VulkanRHIDevice::CreateBuffer(const BufferDesc& desc)
    {
        VkBufferCreateInfo bInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bInfo.size = desc.size;
        switch (desc.usage)
        {
        case RHI_BufferUsage::Vertex:
            bInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        case RHI_BufferUsage::Index:
            bInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        case RHI_BufferUsage::Uniform:
            bInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        case RHI_BufferUsage::Staging:
            bInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        case RHI_BufferUsage::TransferDst:
            bInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        default:
            break;
        }

        VmaAllocationCreateInfo aInfo{};
        if (desc.memoryUsage == MemoryUsage::GPU_Only)
        {
            aInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        }
        else
        {
            aInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            aInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VulkanBuffer vb;
        VK_CHECK(vmaCreateBuffer(m_allocator, &bInfo, &aInfo, &vb.buffer, &vb.allocation, &vb.allocInfo), "VMA buffer alloc failed");
        return m_buffers.Create(vb);
    }

    TextureHandle VulkanRHIDevice::CreateTexture(const TextureDesc& desc)
    {
        VkFormat vkFmt = ToVkFormat(desc.format);
        VkImageCreateInfo iInfo{};
        iInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        iInfo.imageType = VK_IMAGE_TYPE_2D;
        iInfo.extent = { desc.width, desc.height, 1 };
        iInfo.mipLevels = desc.mipLevels;
        iInfo.arrayLayers = desc.arrayLayers;
        iInfo.format = vkFmt;
        iInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        iInfo.usage = ToVkUsage(desc.usage, desc.format);

        VmaAllocationCreateInfo aInfo{};
        aInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VulkanTexture vt;
        vt.format = vkFmt;
        vt.width = desc.width;
        vt.height = desc.height;
        vt.handle = nullptr;
        VK_CHECK(vmaCreateImage(m_allocator, &iInfo, &aInfo, &vt.image, &vt.allocation, nullptr), "VMA image alloc failed");

        VkImageViewCreateInfo vInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vInfo.image = vt.image;
        vInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vInfo.format = vkFmt;
        vInfo.subresourceRange.aspectMask = (desc.format == RHI_Format::D32_SFloat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        vInfo.subresourceRange.levelCount = 1;
        vInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(m_device, &vInfo, nullptr, &vt.view), "Failed to create Image View");

        return m_textures.Create(vt);
    }

    ShaderHandle VulkanRHIDevice::CreateShader(const ShaderDesc& desc)
    {
        VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        info.codeSize = desc.codeSize;
        info.pCode = reinterpret_cast<const uint32_t*>(desc.code);
        VulkanShader vs;
        VK_CHECK(vkCreateShaderModule(m_device, &info, nullptr, &vs.module), "Failed to compile shader module");
        return m_shaders.Create(vs);
    }

    PipelineHandle VulkanRHIDevice::CreateGraphicsPipeline(const PipelineDesc& desc)
    {
        VulkanShader* vs = m_shaders.Get(desc.vertexShader);
        VulkanShader* fs = m_shaders.Get(desc.fragmentShader);

        // 128 + description set
        VkPushConstantRange pcRange{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128 };
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &m_globalDescriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pcRange;

        VulkanPipeline pso;
        VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &pso.layout), "Failed to create pipeline");

        VkPipelineShaderStageCreateInfo shaderStages[2] = {};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vs->module;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fs->module;
        shaderStages[1].pName = "main";

        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = desc.vertexLayout.stride;
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attrDescs;
        for (const auto& attr : desc.vertexLayout.attributes)
        {
            attrDescs.push_back({ attr.location, 0, ToVkFormat(attr.format), attr.offset });
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = attrDescs.size();
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(desc.colorAttachmentFormats.size());
        for (auto& cba : colorBlendAttachments)
        {
            cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            cba.blendEnable = desc.alphaBlendEnable ? VK_TRUE : VK_FALSE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            cba.colorBlendOp = VK_BLEND_OP_ADD;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            cba.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = colorBlendAttachments.size();
        colorBlending.pAttachments = colorBlendAttachments.data();

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicStateInfo.dynamicStateCount = dynamicStates.size();
        dynamicStateInfo.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        std::vector<VkFormat> vkColorFormats;
        for (auto f : desc.colorAttachmentFormats)
            vkColorFormats.push_back(ToVkFormat(f));

        VkPipelineRenderingCreateInfo renderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        renderingInfo.colorAttachmentCount = vkColorFormats.size();
        renderingInfo.pColorAttachmentFormats = vkColorFormats.data();
        renderingInfo.depthAttachmentFormat = ToVkFormat(desc.depthAttachmentFormat);

        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.pNext = &renderingInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = pso.layout;
        pipelineInfo.renderPass = VK_NULL_HANDLE;

        VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pso.pipeline), "Failed to create Graphics Pipeline");

        return m_pipelines.Create(pso);
    }

    void* VulkanRHIDevice::GetMappedData(BufferHandle handle)
    {
        return m_buffers.Get(handle)->allocInfo.pMappedData;
    }

    IRHICommandList* VulkanRHIDevice::AllocateCommandList()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPools[m_currentFrame];
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, &cmd), "Failed to allocate command buffer");
        return new VulkanCommandList(this, cmd);
    }

    VkDescriptorSet VulkanRHIDevice::AllocateTransientDescriptorSet()
    {
        VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = m_descriptorPools[m_currentFrame];
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_globalDescriptorLayout;

        VkDescriptorSet set;
        vkAllocateDescriptorSets(m_device, &allocInfo, &set);
        return set;
    }

    void VulkanRHIDevice::CreateInstance()
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        createInfo.pApplicationInfo = &appInfo;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        // 开启验证层
        const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create Vulkan instance");
    }

    void VulkanRHIDevice::CreateSurface()
    {
        if (!m_windowHandle)
            LOG_ERROR("Vulkan", "No window input");
        auto glfwWindowHandle = static_cast<GLFWwindow*>(m_windowHandle);
        VK_CHECK(glfwCreateWindowSurface(m_instance, glfwWindowHandle, nullptr, &m_surface), "Failed to create window surface");
    }

    void VulkanRHIDevice::PickPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        if (count == 0)
            throw std::runtime_error("No Vulkan physical device found");

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        // TODO: 实现评估方法
        m_physicalDevice = devices[0];
    }

    void VulkanRHIDevice::CreateLogicalDevice()
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, families.data());

        m_graphicsQueueFamily = 0;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                m_graphicsQueueFamily = i;
                break;
            }
        }

        float priority = 1.0f;
        VkDeviceQueueCreateInfo qinfo{};
        qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qinfo.queueFamilyIndex = m_graphicsQueueFamily;
        qinfo.queueCount = 1;
        qinfo.pQueuePriorities = &priority;

        VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        features13.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &qinfo;
        info.pNext = &features13;

        const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = deviceExtensions;

        VK_CHECK(vkCreateDevice(m_physicalDevice, &info, nullptr, &m_device), "vkCreateDevice failed");
        vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    }

    void VulkanRHIDevice::CreateAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = m_physicalDevice;
        allocatorInfo.device = m_device;
        allocatorInfo.instance = m_instance;

        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;

        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_allocator), "Failed to create VMA allocator");
    }

    // TODO: 使用反射实现
    void VulkanRHIDevice::CreateGlobalLayouts()
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        // Uniform Buffer
        for (uint32_t i = 0; i < 4; i++)
        {
            bindings.push_back({ i, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS });
        }

        // Image Sampler
        for (uint32_t i = 4; i < 16; i++)
        {
            bindings.push_back({
                i,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                1,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                nullptr,
            });
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_globalDescriptorLayout), "Failed to create descriptor set layout");

        // 默认采样器
        VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        // samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        // samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        // samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(m_device, &samplerInfo, nullptr, &m_defaultSampler);

        // 为每帧创建一个 descriptor pool，支持 transient 分配
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            std::array<VkDescriptorPoolSize, 2> poolSizes{};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = 64;
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes[1].descriptorCount = 256;

            VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            poolInfo.maxSets = 256;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();

            VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPools[i]), "Failed to create descriptor pool");
        }
    }
    void VulkanRHIDevice::CreateCommandPools()
    {
        VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.queueFamilyIndex = m_graphicsQueueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPools[i]), "Failed to create command pool");
        }
    }

    void VulkanRHIDevice::CreateSwapchain()
    {
        auto glfwWindowHandle = static_cast<GLFWwindow*>(m_windowHandle);
        m_swapchainExtent = { m_width, m_height };

        // TODO: 检查是否支持托惨
        m_swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = MAX_FRAMES_IN_FLIGHT + 1;
        createInfo.imageFormat = m_swapchainFormat;
        createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        createInfo.imageExtent = m_swapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync
        createInfo.clipped = VK_TRUE;

        VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain), "Failed to create swapchain");

        // uint32_t imageCount;
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, nullptr);

        m_swapchainImages.resize(m_imageCount);
        m_swapchainTextures.resize(m_imageCount);
        m_swapchainImageViews.resize(m_imageCount);

        vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, m_swapchainImages.data());

        for (size_t i = 0; i < m_imageCount; i++)
        {
            VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            viewInfo.image = m_swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_swapchainFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]);

            // 打包成 vulkan texture 前端可以调用
            VulkanTexture vt;
            vt.image = m_swapchainImages[i];
            vt.view = m_swapchainImageViews[i];
            vt.format = m_swapchainFormat;
            vt.width = m_swapchainExtent.width;
            vt.height = m_swapchainExtent.height;
            vt.allocation = nullptr;
            vt.handle = nullptr;
            m_swapchainTextures[i] = m_textures.Create(vt);
        }
    }

    void VulkanRHIDevice::CreateSyncObjects()
    {
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        // m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]);
            // vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]);
        }

        m_renderFinishedSemaphores.resize(m_imageCount);
        for (uint32_t i = 0; i < m_imageCount; ++i)
        {
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
        }
    }

    void VulkanRHIDevice::DestroyBuffer(BufferHandle handle)
    {
        std::lock_guard<std::mutex> lock(m_deletionMutex);
        m_bufferDeletionQueue.push_back({ m_absoluteFrame, handle.raw_value });
    }

    void VulkanRHIDevice::DestroyTexture(TextureHandle handle)
    {
        std::lock_guard<std::mutex> lock(m_deletionMutex);
        m_textureDeletionQueue.push_back({ m_absoluteFrame, handle.raw_value });
    }

    void VulkanRHIDevice::FlushDeletionQueue()
    {
        {
            auto it = m_bufferDeletionQueue.begin();
            while (it != m_bufferDeletionQueue.end())
            {
                if (m_absoluteFrame > it->frameIndex + MAX_FRAMES_IN_FLIGHT)
                {
                    BufferHandle h{ it->handleRaw };
                    VulkanBuffer* vb = m_buffers.Get(h);
                    if (vb)
                    {
                        vmaDestroyBuffer(m_allocator, vb->buffer, vb->allocation);
                        m_buffers.Destroy(h);
                    }
                    it = m_bufferDeletionQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        {
            auto it = m_textureDeletionQueue.begin();
            while (it != m_textureDeletionQueue.end())
            {
                if (m_absoluteFrame > it->frameIndex + MAX_FRAMES_IN_FLIGHT)
                {
                    TextureHandle h{ it->handleRaw };
                    VulkanTexture* tex = m_textures.Get(h);
                    if (tex->view)
                    {
                        vkDestroyImageView(m_device, tex->view, nullptr);
                    }
                    if (tex->allocation)
                    {
                        vmaDestroyImage(m_allocator, tex->image, tex->allocation);
                    }
                    it = m_textureDeletionQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    void* VulkanRHIDevice::GetImGuiTextureHandle(TextureHandle handle)
    {
        VulkanTexture* tex = m_textures.Get(handle);
        if (!tex)
            return nullptr;

        // 懒注册
        if (!tex->handle)
        {
            VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(m_defaultSampler, tex->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            tex->handle = static_cast<void*>(ds);
        }
        return tex->handle;
    }

    TextureHandle VulkanRHIDevice::GetActiveSwapchainTexture()
    {
        return m_swapchainTextures[m_currentImageIndex];
    }
} // namespace ChikaEngine::Render