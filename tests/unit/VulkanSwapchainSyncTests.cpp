#include "Vulkan/VulkanSwapchainSyncLifecycle.hpp"
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace
{
    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    template <typename Handle> Handle MakeHandle(uintptr_t value)
    {
        if constexpr (std::is_pointer_v<Handle>)
            return reinterpret_cast<Handle>(value);
        else
            return static_cast<Handle>(value);
    }

    VkSemaphore MakeSemaphore(uintptr_t value)
    {
        return MakeHandle<VkSemaphore>(value);
    }

    void CheckUniqueNonNull(const std::vector<VkSemaphore>& semaphores, const char* message)
    {
        std::unordered_set<VkSemaphore> unique;
        for (VkSemaphore semaphore : semaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
                unique.insert(semaphore);
        }
        Check(unique.size() == semaphores.size(), message);
    }

    void TestImageCountTransitions()
    {
        std::vector<VkSemaphore> semaphores;
        std::vector<VkSemaphore> destroyed;
        uintptr_t nextHandle = 1;

        const auto create = [&](VkSemaphore* semaphore)
        {
            *semaphore = MakeSemaphore(nextHandle++);
            return VK_SUCCESS;
        };
        const auto destroy = [&](VkSemaphore semaphore) { destroyed.push_back(semaphore); };

        Check(ChikaEngine::Render::Detail::RecreateSwapchainSemaphores(semaphores, 3, create, destroy) == VK_SUCCESS, "initial three-image semaphore creation succeeds");
        Check(semaphores.size() == 3, "three swapchain images own three render-finished semaphores");
        Check(destroyed.empty(), "initial creation destroys no semaphore");
        CheckUniqueNonNull(semaphores, "initial semaphore handles are non-null and unique");

        Check(ChikaEngine::Render::Detail::RecreateSwapchainSemaphores(semaphores, 2, create, destroy) == VK_SUCCESS, "three-to-two recreation succeeds");
        Check(semaphores.size() == 2, "three-to-two recreation shrinks the semaphore set");
        Check(destroyed.size() == 3, "three-to-two recreation destroys every old semaphore");
        CheckUniqueNonNull(semaphores, "three-to-two replacement handles are non-null and unique");

        Check(ChikaEngine::Render::Detail::RecreateSwapchainSemaphores(semaphores, 3, create, destroy) == VK_SUCCESS, "two-to-three recreation succeeds");
        Check(semaphores.size() == 3, "two-to-three recreation grows the semaphore set");
        Check(destroyed.size() == 5, "two-to-three recreation destroys every old semaphore");
        CheckUniqueNonNull(semaphores, "two-to-three replacement handles are non-null and unique");

        ChikaEngine::Render::Detail::DestroySwapchainSemaphores(semaphores, destroy);
        Check(semaphores.empty(), "explicit cleanup empties the semaphore set");
        Check(destroyed.size() == 8, "explicit cleanup destroys every live semaphore exactly once");
        CheckUniqueNonNull(destroyed, "every semaphore is destroyed exactly once across image-count transitions");
        ChikaEngine::Render::Detail::DestroySwapchainSemaphores(semaphores, destroy);
        Check(destroyed.size() == 8, "repeated cleanup is idempotent");
    }

    void TestCreationFailureRollsBack()
    {
        std::vector<VkSemaphore> semaphores;
        std::vector<VkSemaphore> destroyed;
        uintptr_t nextHandle = 100;
        const auto destroy = [&](VkSemaphore semaphore) { destroyed.push_back(semaphore); };
        const auto create = [&](VkSemaphore* semaphore)
        {
            *semaphore = MakeSemaphore(nextHandle++);
            return VK_SUCCESS;
        };

        Check(ChikaEngine::Render::Detail::RecreateSwapchainSemaphores(semaphores, 2, create, destroy) == VK_SUCCESS, "failure test creates an initial semaphore set");
        uint32_t createAttempt = 0;
        const auto failOnThirdCreate = [&](VkSemaphore* semaphore)
        {
            if (createAttempt++ == 2)
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            *semaphore = MakeSemaphore(nextHandle++);
            return VK_SUCCESS;
        };

        Check(ChikaEngine::Render::Detail::RecreateSwapchainSemaphores(semaphores, 3, failOnThirdCreate, destroy) == VK_ERROR_OUT_OF_HOST_MEMORY, "semaphore creation propagates the Vulkan failure");
        Check(semaphores.empty(), "failed recreation exposes no partial semaphore set");
        Check(destroyed.size() == 4, "failed recreation destroys the old set and both successful replacements");
        CheckUniqueNonNull(destroyed, "failure rollback destroys each successful handle exactly once");

        Check(ChikaEngine::Render::Detail::RecreateSwapchainSemaphores(semaphores, 0, create, destroy) == VK_ERROR_INITIALIZATION_FAILED, "zero-image swapchain is rejected");
        Check(semaphores.empty(), "zero-image rejection keeps the semaphore set empty");
    }
} // namespace

int main()
{
    TestImageCountTransitions();
    TestCreationFailureRollsBack();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " Vulkan swapchain sync test(s) failed\n";
        return 1;
    }

    std::cout << "Vulkan swapchain sync tests passed\n";
    return 0;
}
