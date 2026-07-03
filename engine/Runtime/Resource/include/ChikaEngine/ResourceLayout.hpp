/*!
 * @file ResourceLayout.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 资源层的 Layout 创建 —— 定义 GPU 侧的数据类型
 * @version 0.1
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
#include "ChikaEngine/ResourceBinder.hpp"
#include "ChikaEngine/math/Bounds.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Resource
{

    struct MeshGPU
    {
        Render::BufferHandle vertexBuffer;
        Render::BufferHandle indexBuffer;
        // FIXME: 实际上可能不是 32bits 或许重新进行一个枚举封装更安全
        uint32_t indexCount = 0;
        bool isUint32 = true;
        Math::Bounds bounds;
    };

    struct TextureGPU
    {
        Render::TextureHandle texture;
        Render::TextureViewHandle defaultView;
        Render::SamplerHandle sampler;
        std::vector<Render::TextureViewHandle> mipViews;
        std::vector<Render::TextureViewHandle> faceViews;
        Render::TextureDimension dimension = Render::TextureDimension::Texture2D;
        Asset::TextureAssetUsage usage = Asset::TextureAssetUsage::Color;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
    };

    /**
     * @brief 保存材质 Pipeline 在逐 Draw 阶段需要更新的动态资源地址。
     *
     * Handle 在材质创建时由 Reflection 解析，Renderer 不再逐 Draw 查询资源名称。
     */
    struct MaterialDrawBindings
    {
        Render::ResourceBindingHandle scene;
        Render::ResourceBindingHandle shadowMap;
        Render::ResourceBindingHandle bones;
        Render::ResourceBindingHandle instances;
        Render::ResourceBindingHandle gpuVisibleInstances;
        Render::ResourceBindingHandle gpuInstances;
        Render::ResourceBindingHandle lights;
    };

    enum class MaterialParameterType
    {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Bool,
    };

    struct MaterialParameterInfo
    {
        std::string name;
        MaterialParameterType type = MaterialParameterType::Float;
        uint32_t componentCount = 1;
        std::vector<float> value;
        std::vector<float> defaultValue;
    };

    struct MaterialParameterValue
    {
        MaterialParameterType type = MaterialParameterType::Float;
        std::vector<float> value;
    };

    struct MaterialParameterRuntime
    {
        MaterialParameterType type = MaterialParameterType::Float;
        uint32_t componentCount = 1;
        uint32_t offset = 0;
        uint32_t size = 0;
        std::vector<float> value;
        std::vector<float> defaultValue;
    };

    struct MaterialGPU
    {
        Render::PipelineHandle pipeline;
        Render::PipelineHandle forwardPipeline;
        Render::PipelineHandle gbufferPipeline;
        Render::PipelineHandle shadowPipeline;
        Render::ShaderHandle vertexShader;
        Render::ShaderHandle fragmentShader;
        Render::ShaderHandle gbufferFragmentShader;
        Render::BufferHandle uboBuffer;
        uint64_t parameterBufferSize = 0;
        std::vector<uint8_t> parameterData;
        std::unordered_map<std::string, MaterialParameterRuntime> parameters;
        MaterialDrawBindings forwardDrawBindings;
        MaterialDrawBindings gbufferDrawBindings;
        MaterialDrawBindings shadowDrawBindings;
        std::vector<Render::ResourceBindingGroup> bindings;
        std::vector<Render::ResourceBindingGroup> shadowBindings;
        bool transparent = false;
        bool masked = false;
        bool ownsPipelineResources = true;
        bool runtimeInstance = false;
    };

} // namespace ChikaEngine::Resource
