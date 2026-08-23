#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

namespace ChikaEngine::Render::Detail
{
    /** @brief 只有真正取得可用 swapchain image 的结果才能继续当前帧。 */
    constexpr bool IsSwapchainAcquireUsable(VkResult result) noexcept
    {
        return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
    }

    /**
     * @brief 销毁全部按 swapchain image 分配的 semaphore，并清空句柄集合。
     *
     * destroySemaphore 由调用方注入，使生命周期逻辑可以在不创建真实 Vulkan device 的情况下测试。
     */
    template <typename DestroySemaphore> void DestroySwapchainSemaphores(std::vector<VkSemaphore>& semaphores, DestroySemaphore&& destroySemaphore)
    {
        for (VkSemaphore semaphore : semaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
                destroySemaphore(semaphore);
        }
        semaphores.clear();
    }

    /**
     * @brief 让 semaphore 集合严格匹配新的 swapchain image 数量。
     *
     * 旧集合总是先销毁；任意一次创建失败时，已创建的新句柄也会回滚，集合保持为空，
     * 从而不会暴露部分初始化或沿用旧 image count 的状态。
     */
    template <typename CreateSemaphore, typename DestroySemaphore> VkResult RecreateSwapchainSemaphores(std::vector<VkSemaphore>& semaphores, uint32_t imageCount, CreateSemaphore&& createSemaphore, DestroySemaphore&& destroySemaphore)
    {
        DestroySwapchainSemaphores(semaphores, destroySemaphore);
        if (imageCount == 0)
            return VK_ERROR_INITIALIZATION_FAILED;

        std::vector<VkSemaphore> replacements(imageCount, VK_NULL_HANDLE);
        uint32_t createdCount = 0;
        for (; createdCount < imageCount; ++createdCount)
        {
            const VkResult result = createSemaphore(&replacements[createdCount]);
            if (result != VK_SUCCESS)
            {
                replacements.resize(createdCount);
                DestroySwapchainSemaphores(replacements, destroySemaphore);
                return result;
            }
        }

        semaphores = std::move(replacements);
        return VK_SUCCESS;
    }
} // namespace ChikaEngine::Render::Detail
