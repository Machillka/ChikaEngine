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

#include "ChikaEngine/RHIResourceHandle.hpp"
#include "ChikaEngine/ResourceBinder.hpp"
#include <cstdint>

namespace ChikaEngine::Resource
{

    struct MeshGPU
    {
        Render::BufferHandle vertexBuffer;
        Render::BufferHandle indexBuffer;
        // FIXME: 实际上可能不是 32bits 或许重新进行一个枚举封装更安全
        uint32_t indexCount = 0;
        bool isUint32 = true;
    };

    struct TextureGPU
    {
        Render::TextureHandle texture;
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
    };

    struct MaterialGPU
    {
        Render::PipelineHandle pipeline;
        Render::PipelineHandle forwardPipeline;
        Render::PipelineHandle gbufferPipeline;
        Render::ShaderHandle vertexShader;
        Render::ShaderHandle fragmentShader;
        Render::ShaderHandle gbufferFragmentShader;
        Render::BufferHandle uboBuffer;
        MaterialDrawBindings forwardDrawBindings;
        MaterialDrawBindings gbufferDrawBindings;
        std::vector<Render::ResourceBindingGroup> bindings;
    };

} // namespace ChikaEngine::Resource
