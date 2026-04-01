/*!
 * @file RHIBackendFactory.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief  创建 RHI 后端的 工厂方法
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

// #include <memory>
#include <ChikaEngine/rhi/Vulkan/VulkanRHIDevice.hpp>

namespace ChikaEngine::Render
{
    enum class RHIBackendTypes
    {
        Auto, // 假装自己实现了一个自动选择后端的高级方法
        Vulkan,
        OpenGL,
        Default, // 然而还是 Default 实在

    };

    // class RHIBackendFactory
    // {
    //   public:
    //     static std::unique_ptr<IRHIDevice> CreateRHIDevice(RHIBackendTypes type)
    //     {
    //         switch (type)
    //         {
    //         case RHIBackendTypes::Vulkan:
    //             return std::make_unique<VulkanRHIDevice>();
    //         case RHIBackendTypes::Auto:
    //         case RHIBackendTypes::OpenGL:
    //         case RHIBackendTypes::Default:
    //         default:
    //             return std::make_unique<VulkanRHIDevice>();
    //         }
    //     }
    // };

} // namespace ChikaEngine::Render