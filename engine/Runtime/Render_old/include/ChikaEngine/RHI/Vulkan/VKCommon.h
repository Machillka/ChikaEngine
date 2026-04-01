/*!
 * @file VKCommon.h
 * @brief Vulkan 公共定义和工具函数
 * @date 2026-03-07
 */
#pragma once

// volk 需要在包含 vulkan.h 之前定义
#define VK_NO_PROTOTYPES
#include <volk.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <stdexcept>
#include <string>
#include <cstdint>

namespace ChikaEngine::Render::Vulkan
{
    /// ========== Vulkan 实例化异常类 ==========
    class VulkanException : public std::runtime_error
    {
      public:
        VulkanException(const std::string& message) : std::runtime_error(message) {}
        VulkanException(const std::string& message, VkResult result) : std::runtime_error(message + " (VkResult: " + std::to_string(result) + ")") {}
    };

/// ========== Vulkan 检查宏 ==========
#define VK_CHECK(result, message) \
    if ((result) != VK_SUCCESS) \
    { \
        throw VulkanException((message), (result)); \
    }

    /// ========== 缓冲内存信息 ==========
    struct BufferMemoryRequirements
    {
        uint32_t memoryTypeIndex = 0;
        VkMemoryPropertyFlags requiredFlags = 0;
        VkMemoryPropertyFlags preferredFlags = 0;
    };

    /// ========== 队列族索引 ==========
    struct QueueFamilyIndices
    {
        uint32_t graphics = UINT32_MAX; // 图形队列族
        uint32_t transfer = UINT32_MAX; // 传输队列族(DMA)
        uint32_t compute = UINT32_MAX;  // 计算队列族
        uint32_t present = UINT32_MAX;  // 呈现队列族

        /// 检查是否找到了所有必需的队列族
        bool IsComplete() const
        {
            return graphics != UINT32_MAX && present != UINT32_MAX;
        }

        /// 检查是否有专用传输队列
        bool HasDedicatedTransfer() const
        {
            return transfer != UINT32_MAX && transfer != graphics;
        }
    };

    /// ========== 交换链支持信息 ==========
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    /// ========== 物理设备信息 ==========
    struct PhysicalDeviceInfo
    {
        VkPhysicalDevice handle = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceMemoryProperties memoryProperties;
        QueueFamilyIndices queueFamilies;
        SwapChainSupportDetails swapChainSupport;
    };

    /// ========== 图形管线配置 ==========
    struct PipelineConfig
    {
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        float lineWidth = 1.0f;
        bool enableDepthTest = true;
        bool enableDepthWrite = true;
        bool enableBlending = false;
        VkBlendOp blendOp = VK_BLEND_OP_ADD;
        VkBlendFactor srcBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        VkBlendFactor dstBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    };

    /// ========== Vulkan 工具函数 ==========
    namespace Utils
    {
        /// 查找合适的内存类型索引
        uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties& memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties);

        /// 创建缓冲区命令缓冲区复制数据
        void CopyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

        /// 执行一次性命令缓冲区
        VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

        void EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer);

        /// 创建着色器模块
        VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint32_t>& code);

        /// 转换清晰色值（RGBA float [0,1]）
        VkClearValue ToClearValue(float r, float g, float b, float a);
    } // namespace Utils

} // namespace ChikaEngine::Render::Vulkan
