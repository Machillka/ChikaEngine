#include "ChikaEngine/ResourceBinder.hpp"

#include <algorithm>

namespace ChikaEngine::Render
{
    namespace
    {
        /**
         * @brief 查找或创建目标 Set 的绑定组。
         *
         * 同一 Set 只能使用一种生命周期，避免把动态资源写入持久 Descriptor。
         */
        ResourceBindingGroup* FindOrCreateGroup(std::vector<ResourceBindingGroup>& groups, uint32_t set, ResourceBindingLifetime lifetime)
        {
            const auto existing = std::ranges::find(groups, set, &ResourceBindingGroup::set);
            if (existing != groups.end())
                return existing->lifetime == lifetime ? &*existing : nullptr;
            groups.push_back({ .set = set, .lifetime = lifetime });
            return &groups.back();
        }
    } // namespace

    /**
     * @brief 将创建阶段的资源名称解析为运行帧可直接消费的 Descriptor 地址。
     */
    ResourceBindingHandle ResolveResourceBinding(const Shader::ShaderProgramInterface& interface, std::string_view resourceName)
    {
        const Shader::ShaderResourceBinding* resource = interface.FindResource(resourceName);
        if (!resource)
            return {};
        return {
            .name = resource->name,
            .set = resource->set,
            .binding = resource->binding,
            .type = resource->type,
            .arrayCount = resource->arrayCount,
        };
    }

    /**
     * @brief 使用已解析地址把纹理写入目标 Descriptor Set 组。
     */
    bool BindTexture(std::vector<ResourceBindingGroup>& groups, const ResourceBindingHandle& binding, TextureHandle texture, ResourceBindingLifetime lifetime, uint32_t arrayElement)
    {
        if (!binding.IsValid())
            return false;
        ResourceBindingGroup* group = FindOrCreateGroup(groups, binding.set, lifetime);
        return group && group->BindTexture(binding, texture, arrayElement);
    }

    /**
     * @brief 使用已解析地址把 Buffer 写入目标 Descriptor Set 组。
     */
    bool BindBuffer(std::vector<ResourceBindingGroup>& groups, const ResourceBindingHandle& binding, BufferHandle buffer, uint64_t offset, uint64_t size, ResourceBindingLifetime lifetime, uint32_t arrayElement)
    {
        if (!binding.IsValid())
            return false;
        ResourceBindingGroup* group = FindOrCreateGroup(groups, binding.set, lifetime);
        return group && group->BindBuffer(binding, buffer, offset, size, arrayElement);
    }

    /**
     * @brief 使用已解析地址把独立 Sampler 写入目标 Descriptor Set 组。
     */
    bool BindSampler(std::vector<ResourceBindingGroup>& groups, const ResourceBindingHandle& binding, SamplerHandle sampler, ResourceBindingLifetime lifetime, uint32_t arrayElement)
    {
        if (!binding.IsValid())
            return false;
        ResourceBindingGroup* group = FindOrCreateGroup(groups, binding.set, lifetime);
        return group && group->BindSampler(binding, sampler, arrayElement);
    }
} // namespace ChikaEngine::Render
