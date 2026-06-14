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
#include "ChikaEngine/shader/ShaderInterface.hpp"
#include <cstdint>
#include <vector>
namespace ChikaEngine::Render
{
    enum class RHI_Format : uint32_t
    {
        Unknown = 0,
        RGBA8_UNorm,
        RGBA8_SRGB,
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

    enum class RHI_BufferUsage : uint32_t
    {
        None = 0,
        Vertex = 1 << 0,
        Index = 1 << 1,
        Uniform = 1 << 2,
        Storage = 1 << 3,
        TransferSrc = 1 << 4,
        TransferDst = 1 << 5,
        Indirect = 1 << 6,
        Staging = TransferSrc,
    };
    inline RHI_BufferUsage operator|(RHI_BufferUsage a, RHI_BufferUsage b)
    {
        return static_cast<RHI_BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline RHI_BufferUsage operator&(RHI_BufferUsage a, RHI_BufferUsage b)
    {
        return static_cast<RHI_BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline RHI_BufferUsage& operator|=(RHI_BufferUsage& a, RHI_BufferUsage b)
    {
        a = a | b;
        return a;
    }

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
        DepthRead,
        DepthWrite,
        ShaderResource,
        StorageRead,
        StorageWrite,
        CopySrc,
        CopyDst,
        VertexBuffer,
        IndexBuffer,
        UniformBuffer,
        IndirectArgument,
        TransferSrc = CopySrc,
        TransferDst = CopyDst,
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

    enum class PrimitiveTopology : uint32_t
    {
        TriangleList,
        LineList,
    };

    enum class CullMode : uint32_t
    {
        None,
        Front,
        Back,
    };

    enum class FrontFace : uint32_t
    {
        CounterClockwise,
        Clockwise,
    };

    enum class CompareOp : uint32_t
    {
        Never,
        Less,
        LessOrEqual,
        Greater,
        Always,
    };

    enum class FilterMode : uint32_t
    {
        Nearest,
        Linear,
    };

    enum class AddressMode : uint32_t
    {
        Repeat,
        ClampToEdge,
        ClampToBorder,
    };

    /**
     * @brief 描述独立 Sampler；Texture View 与采样行为不再绑定。
     */
    struct SamplerDesc
    {
        FilterMode minFilter = FilterMode::Linear;
        FilterMode magFilter = FilterMode::Linear;
        AddressMode addressU = AddressMode::Repeat;
        AddressMode addressV = AddressMode::Repeat;
        AddressMode addressW = AddressMode::Repeat;
        bool anisotropyEnable = false;
        float maxAnisotropy = 1.0f;
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
        uint32_t sampleCount = 1;
        RHI_TextureUsage usage = RHI_TextureUsage::Sampled;
    };

    /**
     * @brief 指定 Texture Barrier 影响的 mip/layer 范围。
     *
     * count 为 0 时表示从 base 开始覆盖资源剩余范围，由后端按实际资源描述解析。
     */
    struct TextureSubresourceRange
    {
        uint32_t baseMipLevel = 0;
        uint32_t mipLevelCount = 0;
        uint32_t baseArrayLayer = 0;
        uint32_t arrayLayerCount = 0;
    };

    /**
     * @brief 描述 Texture 的可绑定子资源视图。
     */
    struct TextureViewDesc
    {
        TextureHandle texture;
        TextureSubresourceRange range;
    };

    /**
     * @brief 指定 Buffer Barrier 影响的字节范围。
     *
     * size 为 UINT64_MAX 时表示从 offset 到 Buffer 末尾。
     */
    struct BufferRange
    {
        uint64_t offset = 0;
        uint64_t size = UINT64_MAX;
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

        /**
         * @brief Pipeline 的唯一资源与 Push Constant 布局来源。
         *
         * Vulkan 后端必须严格按该接口创建布局，不再使用全局固定 Descriptor Layout。
         */
        Shader::ShaderProgramInterface shaderInterface;
        VertexLayout vertexLayout;

        bool depthTest = true;
        bool depthWrite = true;
        bool alphaBlendEnable = false;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
        CullMode cullMode = CullMode::None;
        FrontFace frontFace = FrontFace::CounterClockwise;
        CompareOp depthCompare = CompareOp::LessOrEqual;

        std::vector<RHI_Format> colorAttachmentFormats;
        RHI_Format depthAttachmentFormat = RHI_Format::D32_SFloat;
    };

    /**
     * @brief 描述 Compute Pipeline；布局完全由 Compute Shader Reflection 提供。
     */
    struct ComputePipelineDesc
    {
        ShaderHandle computeShader;
        Shader::ShaderProgramInterface shaderInterface;
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
