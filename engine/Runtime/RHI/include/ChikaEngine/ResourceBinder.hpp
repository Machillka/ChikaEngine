/*!
 * @file ResourceBinder.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 资源绑定的前端数据说明
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "RHIResourceHandle.hpp"
#include "ChikaEngine/shader/ShaderInterface.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace ChikaEngine::Render
{
    /**
     * @brief 区分每帧临时 Descriptor 与可跨 Draw 复用的静态 Descriptor。
     */
    enum class ResourceBindingLifetime : uint32_t
    {
        Transient,
        Persistent,
    };

    /**
     * @brief 保存 Shader Interface 中一次解析后的稳定 Descriptor 地址。
     *
     * 资源名称仅用于诊断；运行帧使用 set/binding/type，不再重复按名称搜索 Reflection。
     */
    struct ResourceBindingHandle
    {
        std::string name;
        uint32_t set = 0;
        uint32_t binding = 0;
        Shader::ShaderDescriptorType type = Shader::ShaderDescriptorType::UniformBuffer;
        uint32_t arrayCount = 0;

        bool IsValid() const
        {
            return arrayCount > 0;
        }
    };

    /**
     * @brief 保存一个明确 set/binding/type 的资源绑定组。
     *
     * 组中的 binding 只能来自 Shader Reflection，后端不再根据资源种类猜测 Descriptor 类型。
     */
    struct ResourceBindingGroup
    {
        struct TextureBind
        {
            std::string name;
            uint32_t binding = 0;
            Shader::ShaderDescriptorType type = Shader::ShaderDescriptorType::CombinedImageSampler;
            uint32_t arrayElement = 0;
            TextureHandle tex;
        };
        struct BufferBind
        {
            std::string name;
            uint32_t binding = 0;
            Shader::ShaderDescriptorType type = Shader::ShaderDescriptorType::UniformBuffer;
            uint32_t arrayElement = 0;
            BufferHandle buf;
            uint64_t offset = 0;
            uint64_t size = 0;
        };
        struct SamplerBind
        {
            std::string name;
            uint32_t binding = 0;
            uint32_t arrayElement = 0;
            SamplerHandle sampler;
        };

        uint32_t set = 0;
        ResourceBindingLifetime lifetime = ResourceBindingLifetime::Transient;
        std::vector<TextureBind> textures;
        std::vector<BufferBind> buffers;
        std::vector<SamplerBind> samplers;

        /**
         * @brief 使用反射 Descriptor 绑定纹理，并校验资源属于当前 Set。
         */
        bool BindTexture(const ResourceBindingHandle& resource, TextureHandle texture, uint32_t arrayElement = 0)
        {
            const bool isTexture = resource.type == Shader::ShaderDescriptorType::CombinedImageSampler || resource.type == Shader::ShaderDescriptorType::SampledImage || resource.type == Shader::ShaderDescriptorType::StorageImage;
            if (!resource.IsValid() || resource.set != set || !isTexture || arrayElement >= resource.arrayCount)
                return false;
            textures.push_back({ resource.name, resource.binding, resource.type, arrayElement, texture });
            return true;
        }

        /**
         * @brief 使用反射 Descriptor 绑定 Buffer，并保留明确 Descriptor 类型。
         */
        bool BindBuffer(const ResourceBindingHandle& resource, BufferHandle buffer, uint64_t offset, uint64_t size, uint32_t arrayElement = 0)
        {
            const bool isBuffer = resource.type == Shader::ShaderDescriptorType::UniformBuffer || resource.type == Shader::ShaderDescriptorType::StorageBuffer;
            if (!resource.IsValid() || resource.set != set || !isBuffer || arrayElement >= resource.arrayCount)
                return false;
            buffers.push_back({ resource.name, resource.binding, resource.type, arrayElement, buffer, offset, size });
            return true;
        }

        /**
         * @brief 绑定独立 Sampler；当前 Vulkan 后端使用默认 Sampler，Handle 为未来 Sampler 资源保留。
         */
        bool BindSampler(const ResourceBindingHandle& resource, SamplerHandle sampler = {}, uint32_t arrayElement = 0)
        {
            if (!resource.IsValid() || resource.set != set || resource.type != Shader::ShaderDescriptorType::Sampler || arrayElement >= resource.arrayCount)
                return false;
            samplers.push_back({ resource.name, resource.binding, arrayElement, sampler });
            return true;
        }
    };

    /**
     * @brief 在 Pipeline/Material 创建阶段把资源名称解析为稳定 Descriptor Handle。
     *
     * 无效名称返回空 Handle，使可选 Shader 资源无需进入运行帧名称查询。
     */
    ResourceBindingHandle ResolveResourceBinding(const Shader::ShaderProgramInterface& interface, std::string_view resourceName);

    /**
     * @brief 使用已解析 Handle 写入对应 Set 的资源组。
     */
    bool BindTexture(std::vector<ResourceBindingGroup>& groups, const ResourceBindingHandle& binding, TextureHandle texture, ResourceBindingLifetime lifetime = ResourceBindingLifetime::Transient, uint32_t arrayElement = 0);
    bool BindBuffer(std::vector<ResourceBindingGroup>& groups, const ResourceBindingHandle& binding, BufferHandle buffer, uint64_t offset, uint64_t size, ResourceBindingLifetime lifetime = ResourceBindingLifetime::Transient, uint32_t arrayElement = 0);
    bool BindSampler(std::vector<ResourceBindingGroup>& groups, const ResourceBindingHandle& binding, SamplerHandle sampler = {}, ResourceBindingLifetime lifetime = ResourceBindingLifetime::Persistent, uint32_t arrayElement = 0);

} // namespace ChikaEngine::Render
