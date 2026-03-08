/*!
 * @file VKResources.cpp
 * @brief Vulkan RHI 资源实现
 * @date 2026-03-07
 */

#include "ChikaEngine/RHI/Vulkan/VKResources.h"
#include "ChikaEngine/RHI/Vulkan/VKCommon.h"
#include "ChikaEngine/debug/log_macros.h"
#include <cstring>

namespace ChikaEngine::Render::Vulkan
{
    // ========== VKBuffer 实现 ==========
    VKBuffer::~VKBuffer()
    {
        if (device_ != VK_NULL_HANDLE && buffer_ != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device_, buffer_, nullptr);
            if (memory_ != VK_NULL_HANDLE)
            {
                vkFreeMemory(device_, memory_, nullptr);
            }
        }
    }

    void VKBuffer::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const void* data)
    {
        device_ = device;
        size_ = size;

        // 创建缓冲区
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer_), "Failed to create buffer");

        // 查询内存需求
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device_, buffer_, &memRequirements);

        // 分配内存
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        uint32_t memoryTypeIndex = Utils::FindMemoryType(memProperties, memRequirements.memoryTypeBits, properties);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &memory_), "Failed to allocate memory");

        // 绑定内存
        vkBindBufferMemory(device_, buffer_, memory_, 0);

        // 如果有初始数据，复制到缓冲区
        if (data != nullptr && (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
        {
            void* mappedMemory;
            vkMapMemory(device_, memory_, 0, size, 0, &mappedMemory);
            std::memcpy(mappedMemory, data, size);
            vkUnmapMemory(device_, memory_);
        }
    }

    void VKBuffer::Update(VkDevice device, VkQueue queue, VkCommandPool commandPool, const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        // 如果是可见内存，直接映射更新
        if (size_ >= offset + size)
        {
            // 创建临时缓冲区用于上传
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;

            VkBufferCreateInfo stagingInfo{};
            stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingInfo.size = size;
            stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            vkCreateBuffer(device, &stagingInfo, nullptr, &stagingBuffer);

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(vkGetPhysicalDeviceForDevice(device), &memProperties);

            uint32_t memoryTypeIndex = Utils::FindMemoryType(memProperties, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = memoryTypeIndex;

            vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
            vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

            // 复制数据到临时缓冲区
            void* mappedMemory;
            vkMapMemory(device, stagingMemory, 0, size, 0, &mappedMemory);
            std::memcpy(mappedMemory, data, size);
            vkUnmapMemory(device, stagingMemory);

            // 复制到目标缓冲区
            Utils::CopyBuffer(device, commandPool, queue, stagingBuffer, buffer_, size);

            // 清理临时缓冲区
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
        }
    }

    // ========== VKVertexArray 实现 ==========
    VKVertexArray::~VKVertexArray()
    {
        // 注意：VKVertexArray 不拥有缓冲区，缓冲区由设备管理
    }

    void VKVertexArray::SetupMesh(VKBuffer* vertexBuffer, VKBuffer* indexBuffer, uint32_t indexCount, IndexType indexType)
    {
        vertexBuffer_ = vertexBuffer;
        indexBuffer_ = indexBuffer;
        indexCount_ = indexCount;
        indexType_ = indexType;
        isLineData_ = false;
    }

    void VKVertexArray::SetupLines(VKBuffer* vertexBuffer, uint32_t vertexCount)
    {
        vertexBuffer_ = vertexBuffer;
        vertexCount_ = vertexCount;
        isLineData_ = true;
    }

    // ========== VKTexture2D 实现 ==========
    VKTexture2D::~VKTexture2D()
    {
        if (device_ != VK_NULL_HANDLE)
        {
            if (sampler_ != VK_NULL_HANDLE)
                vkDestroySampler(device_, sampler_, nullptr);
            if (imageView_ != VK_NULL_HANDLE)
                vkDestroyImageView(device_, imageView_, nullptr);
            if (image_ != VK_NULL_HANDLE)
                vkDestroyImage(device_, image_, nullptr);
            if (imageMemory_ != VK_NULL_HANDLE)
                vkFreeMemory(device_, imageMemory_, nullptr);
        }
    }

    void VKTexture2D::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, int width, int height, int channels, const void* data, bool sRGB)
    {
        device_ = device;
        width_ = width;
        height_ = height;

        // 确定像素格式
        VkFormat format = VK_FORMAT_UNDEFINED;
        switch (channels)
        {
        case 1:
            format = VK_FORMAT_R8_UNORM;
            break;
        case 2:
            format = VK_FORMAT_R8G8_UNORM;
            break;
        case 3:
            format = VK_FORMAT_R8G8B8_UNORM;
            break;
        case 4:
            format = sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            break;
        default:
            throw VulkanException("Unsupported texture channel count");
        }

        VkDeviceSize imageSize = width * height * channels;

        // 创建临时缓冲区（Staging Buffer）
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer), "Failed to create staging buffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        uint32_t memoryTypeIndex = Utils::FindMemoryType(memProperties, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory), "Failed to allocate staging memory");

        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

        // 复制数据到临时缓冲区
        void* mappedMemory;
        vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mappedMemory);
        std::memcpy(mappedMemory, data, imageSize);
        vkUnmapMemory(device, stagingMemory);

        // 创建图像
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image_), "Failed to create image");

        // 分配图像内存
        vkGetImageMemoryRequirements(device, image_, &memRequirements);

        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Utils::FindMemoryType(memProperties, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory_), "Failed to allocate image memory");

        vkBindImageMemory(device, image_, imageMemory_, 0);

        // 过渡图像布局
        TransitionImageLayout(commandPool, queue, image_, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // 从缓冲区复制到图像
        VkCommandBuffer commandBuffer = Utils::BeginSingleTimeCommands(device, commandPool);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowPitch = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        Utils::EndSingleTimeCommands(device, commandPool, queue, commandBuffer);

        // 过渡到着色器读取布局
        TransitionImageLayout(commandPool, queue, image_, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // 清理临时缓冲区
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);

        // 创建图像视图和采样器
        CreateImageView(format);
        CreateSampler();
    }

    std::uintptr_t VKTexture2D::Handle() const
    {
        return reinterpret_cast<std::uintptr_t>(imageView_);
    }

    VkDescriptorImageInfo VKTexture2D::GetDescriptorInfo() const
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = imageView_;
        imageInfo.sampler = sampler_;
        return imageInfo;
    }

    void VKTexture2D::CreateImageView(VkFormat format)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &imageView_), "Failed to create image view");
    }

    void VKTexture2D::CreateSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_), "Failed to create sampler");
    }

    void VKTexture2D::TransitionImageLayout(VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer commandBuffer = Utils::BeginSingleTimeCommands(device_, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw VulkanException("Unsupported layout transition");
        }

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        Utils::EndSingleTimeCommands(device_, commandPool, queue, commandBuffer);
    }

    // ========== VKTextureCube 实现 ==========
    VKTextureCube::~VKTextureCube()
    {
        if (device_ != VK_NULL_HANDLE)
        {
            if (sampler_ != VK_NULL_HANDLE)
                vkDestroySampler(device_, sampler_, nullptr);
            if (imageView_ != VK_NULL_HANDLE)
                vkDestroyImageView(device_, imageView_, nullptr);
            if (image_ != VK_NULL_HANDLE)
                vkDestroyImage(device_, image_, nullptr);
            if (imageMemory_ != VK_NULL_HANDLE)
                vkFreeMemory(device_, imageMemory_, nullptr);
        }
    }

    void VKTextureCube::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, int width, int height, int channels, const std::array<const void*, 6>& facesData, bool sRGB)
    {
        device_ = device;
        width_ = width;
        height_ = height;

        VkFormat format = sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        VkDeviceSize faceSize = width * height * 4; // 假设总是 RGBA
        VkDeviceSize totalSize = faceSize * 6;

        // 创建临时缓冲区并上传数据
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = totalSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer), "Failed to create staging buffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        uint32_t memoryTypeIndex = Utils::FindMemoryType(memProperties, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory), "Failed to allocate staging memory");

        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

        void* mappedMemory;
        vkMapMemory(device, stagingMemory, 0, totalSize, 0, &mappedMemory);

        // 复制六个面的数据
        for (int i = 0; i < 6; ++i)
        {
            std::memcpy(reinterpret_cast<char*>(mappedMemory) + i * faceSize, facesData[i], faceSize);
        }

        vkUnmapMemory(device, stagingMemory);

        // 创建立方体纹理
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 6;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image_), "Failed to create cubemap image");

        vkGetImageMemoryRequirements(device, image_, &memRequirements);

        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Utils::FindMemoryType(memProperties, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory_), "Failed to allocate image memory");

        vkBindImageMemory(device, image_, imageMemory_, 0);

        // 过渡布局并复制数据
        TransitionImageLayout(commandPool, queue, image_, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkCommandBuffer commandBuffer = Utils::BeginSingleTimeCommands(device, commandPool);

        for (uint32_t i = 0; i < 6; ++i)
        {
            VkBufferImageCopy region{};
            region.bufferOffset = i * faceSize;
            region.bufferRowPitch = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = i;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        Utils::EndSingleTimeCommands(device, commandPool, queue, commandBuffer);

        TransitionImageLayout(commandPool, queue, image_, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);

        CreateImageView(format);
        CreateSampler();
    }

    void* VKTextureCube::Handle() const
    {
        return reinterpret_cast<void*>(imageView_);
    }

    VkDescriptorImageInfo VKTextureCube::GetDescriptorInfo() const
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = imageView_;
        imageInfo.sampler = sampler_;
        return imageInfo;
    }

    void VKTextureCube::CreateImageView(VkFormat format)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 6;

        VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &imageView_), "Failed to create cubemap image view");
    }

    void VKTextureCube::CreateSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_), "Failed to create cubemap sampler");
    }

    void VKTextureCube::TransitionImageLayout(VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer commandBuffer = Utils::BeginSingleTimeCommands(device_, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 6;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw VulkanException("Unsupported layout transition");
        }

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        Utils::EndSingleTimeCommands(device_, commandPool, queue, commandBuffer);
    }

    // ========== VKPipeline 实现 ==========
    VKPipeline::~VKPipeline()
    {
        if (device_ != VK_NULL_HANDLE && pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device_, pipeline_, nullptr);
        }
        if (device_ != VK_NULL_HANDLE && layout_ != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device_, layout_, nullptr);
        }
    }

    void VKPipeline::Initialize(VkDevice device, const PipelineConfig& config, const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, VkExtent2D extent)
    {
        device_ = device;
        layout_ = config.layout;

        // 创建管线
        // TODO: 实现完整的管线创建，包括顶点输入、光栅化等
    }

    void VKPipeline::Bind()
    {
        // 绑定管线到命令缓冲区
        // TODO: 实现管线绑定
    }

    void VKPipeline::SetUniformFloat(const char* name, float v)
    {
        // TODO: 实现 UBO 更新
    }

    void VKPipeline::SetUniformVec3(const char* name, const std::array<float, 3>& v)
    {
        // TODO: 实现 UBO 更新
    }

    void VKPipeline::SetUniformVec4(const char* name, const std::array<float, 4>& v)
    {
        // TODO: 实现 UBO 更新
    }

    void VKPipeline::SetUniformMat4(const char* name, const Math::Mat4& v)
    {
        // TODO: 实现 UBO 更新
    }

    void VKPipeline::SetUniformTextureCube(const char* name, IRHITextureCube* tex, int slot)
    {
        // TODO: 实现纹理绑定
    }

    void VKPipeline::SetUniformTexture(const char* name, IRHITexture2D* tex, int slot)
    {
        // TODO: 实现纹理绑定
    }

    // ========== VKRenderTarget 实现 ==========
    VKRenderTarget::~VKRenderTarget()
    {
        if (device_ != VK_NULL_HANDLE && framebuffer_ != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device_, framebuffer_, nullptr);
        }
        if (device_ != VK_NULL_HANDLE && renderPass_ != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device_, renderPass_, nullptr);
        }
    }

    void VKRenderTarget::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, int width, int height)
    {
        device_ = device;
        width_ = width;
        height_ = height;

        // 创建颜色纹理
        colorTexture_ = std::make_unique<VKTexture2D>();
        // TODO: 初始化颜色纹理

        // 创建深度纹理
        depthTexture_ = std::make_unique<VKTexture2D>();
        // TODO: 初始化深度纹理

        CreateRenderPass();
        CreateFramebuffer();
    }

    void VKRenderTarget::Bind()
    {
        // 绑定渲染目标
        // TODO: 实现渲染目标绑定
    }

    void VKRenderTarget::UnBind()
    {
        // 解绑渲染目标
        // TODO: 实现渲染目标解绑
    }

    IRHITexture2D* VKRenderTarget::GetColorTexture()
    {
        return colorTexture_.get();
    }

    void VKRenderTarget::CreateRenderPass()
    {
        // TODO: 创建渲染通道
    }

    void VKRenderTarget::CreateFramebuffer()
    {
        // TODO: 创建帧缓冲区
    }

} // namespace ChikaEngine::Render::Vulkan
