/*!
 * @file RHIDesc.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 定义 RHI 相关资源——包括描述符
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "RHIResourceHandle.hpp"
#include <cstdint>
#include <vector>
namespace ChikaEngine::Render
{
    enum class RHI_Format : uint32_t
    {
        Unknown = 0,
        RGBA8_UNorm,
        BGRA8_UNorm,
        RGBA16_Float,
        RGBA32_Float,
        R32_Float,
        D32_SFloat,
        RGB32_Float,
        RG32_Float,
        D24S8,
        RGBA32_SInt,
    };

    // TODO: 实现 位运算
    enum class RHI_BufferUsage : uint32_t
    {
        Vertex,
        Index,
        Uniform,
        Storage,
        Staging,
        TransferDst
    };

    enum class RHI_TextureUsage : uint32_t
    {
        None = 0,
        ColorAttachment = 1 << 1,
        DepthStencilAttachment = 1 << 2,
        Sampled = 1 << 3,
        Storage = 1 << 4,
        Presentable = 1 << 5
    };
    inline RHI_TextureUsage operator|(RHI_TextureUsage a, RHI_TextureUsage b)
    {
        return static_cast<RHI_TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline RHI_TextureUsage operator&(RHI_TextureUsage a, RHI_TextureUsage b)
    {
        return static_cast<RHI_TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline RHI_TextureUsage& operator|=(RHI_TextureUsage& a, RHI_TextureUsage b)
    {
        a = a | b;
        return a;
    }

    inline RHI_TextureUsage& operator&=(RHI_TextureUsage& a, RHI_TextureUsage b)
    {
        a = a & b;
        return a;
    }
    // 枚举 Shader 的类型
    enum class RHI_ShaderStage : uint32_t
    {
        Vertex,
        Fragment,
        Compute
    };

    // 内存使用意图
    enum class MemoryUsage : uint32_t
    {
        GPU_Only,  // 纯显存 (纹理, 静态模型)
        CPU_To_GPU // CPU 可写，GPU 可读 (Uniform Buffers, Staging)
    };

    // 资源状态
    enum class ResourceState
    {
        Undefined,
        RenderTarget,
        DepthWrite,
        ShaderResource,
        TransferDst,
        Present
    };

    enum class LoadOp
    {
        Load,
        Clear,
        DontCare
    };
    enum class StoreOp
    {
        Store,
        DontCare
    };

    struct BufferDesc
    {
        uint64_t size = 0;
        RHI_BufferUsage usage = RHI_BufferUsage::Vertex;
        MemoryUsage memoryUsage = MemoryUsage::GPU_Only;
    };

    struct TextureDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        RHI_Format format = RHI_Format::RGBA8_UNorm;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        RHI_TextureUsage usage = RHI_TextureUsage::Sampled;
    };

    struct VertexAttribute
    {
        uint32_t location;
        RHI_Format format;
        uint32_t offset;
    };

    // 顶点描述符
    struct VertexLayout
    {
        uint32_t stride;
        std::vector<VertexAttribute> attributes;
    };

    /*!
     * @brief  单个管线描述
     * 需要 vert 和 frag shader
     * 同时需要指定 顶点布局
     *
     * @author Machillka (machillka2007@gmail.com)
     * @date 2026-03-16
     */
    struct PipelineDesc
    {
        ShaderHandle vertexShader;
        ShaderHandle fragmentShader;

        VertexLayout vertexLayout;

        bool depthTest = true;
        bool depthWrite = true;
        bool alphaBlendEnable = false;

        std::vector<RHI_Format> colorAttachmentFormats;
        RHI_Format depthAttachmentFormat = RHI_Format::D32_SFloat;
    };

    struct ShaderDesc
    {
        RHI_ShaderStage stage;
        const uint8_t* code;
        size_t codeSize;
    };
    struct RenderingAttachment
    {
        TextureHandle texture;
        LoadOp loadOp = LoadOp::Clear;
        StoreOp storeOp = StoreOp::Store;
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    };
} // namespace ChikaEngine::Render
