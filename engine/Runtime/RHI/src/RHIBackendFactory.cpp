#include "ChikaEngine/rhi/RHIBackendFactory.hpp"
#include "ChikaEngine/rhi/Vulkan/VulkanRHIDevice.hpp"
#include <stdexcept>

namespace ChikaEngine::Render
{
    std::unique_ptr<IRHIDevice> RHIBackendFactory::CreateRHIDevice(RHIBackendTypes type, const RHI_InitParams& params)
    {
        switch (type)
        {
        case RHIBackendTypes::Auto:
        case RHIBackendTypes::Default:
        case RHIBackendTypes::Vulkan:
            return std::make_unique<VulkanRHIDevice>(params);
        case RHIBackendTypes::OpenGL:
            throw std::runtime_error("OpenGL RHI backend is not implemented");
        default:
            throw std::runtime_error("Unknown RHI backend type");
        }
    }
} // namespace ChikaEngine::Render
