#include "ChikaEngine/ResourceManager.hpp"
#include "ChikaEngine/AssetHandle.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <span>

namespace ChikaEngine::Resource
{
    namespace
    {
        Render::VertexLayout BuildDefaultVertexLayout()
        {
            return {
                .stride = sizeof(Asset::VertexData),
                .attributes = {
                    {
                        0,
                        Render::RHI_Format::RGB32_Float,
                        offsetof(Asset::VertexData, position),
                    },
                    {
                        1,
                        Render::RHI_Format::RGB32_Float,
                        offsetof(Asset::VertexData, normal),
                    },
                    {
                        2,
                        Render::RHI_Format::RG32_Float,
                        offsetof(Asset::VertexData, uv),
                    },
                    {
                        3,
                        Render::RHI_Format::RGBA32_SInt,
                        offsetof(Asset::VertexData, boneIndices),
                    },
                    {
                        4,
                        Render::RHI_Format::RGBA32_Float,
                        offsetof(Asset::VertexData, boneWeights),
                    },
                },
            };
        }
    } // namespace

    ResourceManager::ResourceManager(Render::IRHIDevice& rhi, Asset::AssetManager& assetManager) : m_rhi(rhi), m_assetManager(assetManager)
    {
        m_assetReloadSubscription = m_assetManager.SubscribeReload([this](const Asset::AssetReloadEvent&) { UnloadAll(); });
    }

    ResourceManager::~ResourceManager()
    {
        m_assetManager.UnsubscribeReload(m_assetReloadSubscription);
        UnloadAll();
    }

    MeshHandle ResourceManager::UploadMesh(Asset::MeshHandle assetHandle)
    {
        if (m_meshCache.contains(assetHandle))
            return m_meshCache[assetHandle];
        return _UploadMesh(assetHandle);
    }

    TextureHandle ResourceManager::UploadTexture(Asset::TextureHandle assetHandle)
    {
        if (m_textureCache.contains(assetHandle))
            return m_textureCache[assetHandle];
        return _UploadTexture(assetHandle);
    }

    MaterialHandle ResourceManager::UploadMaterial(Asset::MaterialHandle assetHandle)
    {
        if (m_materialCache.contains(assetHandle))
            return m_materialCache[assetHandle];
        return _UploadMaterial(assetHandle);
    }

    /*!
     * @brief 快速录制上传命令
     *
     * @param  recordFunc
     * @author Machillka (machillka2007@gmail.com)
     * @date 2026-03-31
     */
    // NOTE: 感觉这样做职责不清晰, Resource 应当是 Adaptor 层, 使得 Asset -> Render 并且只知道资源, 不应当知道如何录制提交, 这个命令应当放到 Renderer 中做统一管理
    // FIXME: Immediate 也不是直接执行, 因为 RHI 层做的也是延迟提交 (
    void ResourceManager::SubmitImmediate(const std::function<void(Render::IRHICommandList*)>& recordFunc)
    {
        Render::IRHICommandList* cmd = m_rhi.AllocateCommandList();
        cmd->Begin();
        recordFunc(cmd);
        cmd->End();
        m_rhi.Submit(cmd);
    }

    MeshHandle ResourceManager::_UploadMesh(Asset::MeshHandle assetHandle)
    {
        const Asset::MeshData* data = m_assetManager.GetMesh(assetHandle);
        if (!data)
            return MeshHandle::Invalid();

        std::span<const Asset::VertexData> vertices = data->vertices;
        std::span<const uint32_t> indices = data->indices;

        uint64_t vSize = vertices.size_bytes();
        uint64_t iSize = indices.size_bytes();

        Render::BufferDesc vDesc{
            .size = vSize,
            .usage = Render::RHI_BufferUsage::Vertex,
            .memoryUsage = Render::MemoryUsage::GPU_Only,
        };

        Render::BufferDesc iDesc{
            .size = iSize,
            .usage = Render::RHI_BufferUsage::Index,
            .memoryUsage = Render::MemoryUsage::GPU_Only,
        };

        Render::BufferHandle vbo = m_rhi.CreateBuffer(vDesc);
        Render::BufferHandle ibo = m_rhi.CreateBuffer(iDesc);

        // 资源名称使用资产路径作为稳定上下文，便于验证层错误直接定位到源资产。
        const std::string meshDebugName = data->path.empty() ? "Mesh.Unnamed" : "Mesh." + data->path;
        m_rhi.SetDebugName(vbo, meshDebugName + ".VertexBuffer");
        m_rhi.SetDebugName(ibo, meshDebugName + ".IndexBuffer");

        Render::BufferDesc stagingVDesc{
            .size = vSize,
            .usage = Render::RHI_BufferUsage::Staging,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };
        Render::BufferDesc stagingIDesc{
            .size = iSize,
            .usage = Render::RHI_BufferUsage::Staging,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };

        Render::BufferHandle stagingV = m_rhi.CreateBuffer(stagingVDesc);
        Render::BufferHandle stagingI = m_rhi.CreateBuffer(stagingIDesc);
        m_rhi.SetDebugName(stagingV, meshDebugName + ".VertexStaging");
        m_rhi.SetDebugName(stagingI, meshDebugName + ".IndexStaging");

        std::memcpy(m_rhi.GetMappedData(stagingV), vertices.data(), vSize);
        std::memcpy(m_rhi.GetMappedData(stagingI), indices.data(), iSize);

        {
            std::lock_guard<std::mutex> lock(m_uploadMutex);
            m_pendingBufferUploads.push_back({ stagingV, vbo, vSize });
            m_pendingBufferUploads.push_back({ stagingI, ibo, iSize });
        }

        // SubmitImmediate(
        //     [&](Render::IRHICommandList* cmd)
        //     {
        //         cmd->CopyBuffer(stagingV, vbo, vSize);
        //         cmd->CopyBuffer(stagingI, ibo, iSize);
        //     });

        m_rhi.DestroyBuffer(stagingV);
        m_rhi.DestroyBuffer(stagingI);

        MeshGPU mesh{
            .vertexBuffer = vbo,
            .indexBuffer = ibo,
            .indexCount = static_cast<uint32_t>(indices.size()),
            .isUint32 = true,
        };

        MeshHandle handle = m_meshes.Create(mesh);
        m_meshCache[assetHandle] = handle;
        return handle;
    }

    TextureHandle ResourceManager::_UploadTexture(Asset::TextureHandle assetHandle)
    {
        const Asset::TextureData* data = m_assetManager.GetTexture(assetHandle);

        if (!data)
            return TextureHandle::Invalid();

        uint64_t imageSize = data->width * data->height * data->channels;
        Render::TextureDesc desc{
            .width = data->width,
            .height = data->height,
            .format = Render::RHI_Format::RGBA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::Sampled,
        };
        Render::TextureHandle tex = m_rhi.CreateTexture(desc);
        const std::string textureDebugName = data->path.empty() ? "Texture.Unnamed" : "Texture." + data->path;
        m_rhi.SetDebugName(tex, textureDebugName);

        Render::BufferDesc stagingDesc{
            .size = imageSize,
            .usage = Render::RHI_BufferUsage::Staging,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };
        Render::BufferHandle staging = m_rhi.CreateBuffer(stagingDesc);
        m_rhi.SetDebugName(staging, textureDebugName + ".Staging");
        std::memcpy(m_rhi.GetMappedData(staging), data->pixels.data(), imageSize);

        {
            std::lock_guard<std::mutex> lock(m_uploadMutex);
            m_pendingTextureUploads.push_back({ staging, tex, data->width, data->height });
        }

        // SubmitImmediate(
        //     [&](Render::IRHICommandList* cmd)
        //     {
        //         cmd->InsertTextureBarrier(tex, Render::ResourceState::Undefined, Render::ResourceState::TransferDst);
        //         cmd->CopyBufferToTexture(staging, tex, data->width, data->height);
        //         cmd->InsertTextureBarrier(tex, Render::ResourceState::TransferDst, Render::ResourceState::ShaderResource);
        //     });

        m_rhi.DestroyBuffer(staging);

        TextureHandle handle = m_textures.Create({ .texture = tex });
        m_textureCache[assetHandle] = handle;
        return handle;
    }

    MaterialHandle ResourceManager::_UploadMaterial(Asset::MaterialHandle assetHandle)
    {
        const Asset::MaterialData* materialData = m_assetManager.GetMaterial(assetHandle);
        if (!materialData)
            return MaterialHandle::Invalid();

        Asset::ShaderTemplateHandle tmplHandle = m_assetManager.LoadShaderTemplate(materialData->shaderTemplatePath);
        const Asset::ShaderTemplateData* tmplData = m_assetManager.GetShaderTemplate(tmplHandle);

        Asset::ShaderHandle vsAsset = m_assetManager.LoadShader(tmplData->vertexShaderPath);
        Asset::ShaderHandle fsAsset = m_assetManager.LoadShader(tmplData->fragmentShaderPath);
        const auto* vsSpirv = m_assetManager.GetShader(vsAsset);
        const auto* fsSpirv = m_assetManager.GetShader(fsAsset);

        Render::ShaderHandle vs = m_rhi.CreateShader({
            .stage = Render::RHI_ShaderStage::Vertex,
            .code = vsSpirv->spirv.data(),
            .codeSize = vsSpirv->spirv.size(),
        });

        Render::ShaderHandle fs = m_rhi.CreateShader({
            .stage = Render::RHI_ShaderStage::Fragment,
            .code = fsSpirv->spirv.data(),
            .codeSize = fsSpirv->spirv.size(),
        });

        // Material 名称统一作为 Shader、Pipeline 和参数缓冲的前缀，方便在 GPU 捕获中按材质分组。
        const std::string materialDebugName = materialData->name.empty() ? "Material.Unnamed" : "Material." + materialData->name;
        m_rhi.SetDebugName(vs, materialDebugName + ".VertexShader");
        m_rhi.SetDebugName(fs, materialDebugName + ".FragmentShader");

        Render::PipelineDesc pipelineDesc{ .vertexShader = vs, .fragmentShader = fs, .vertexLayout = BuildDefaultVertexLayout(), .depthTest = true, .depthWrite = true, .alphaBlendEnable = false };
        pipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::BGRA8_UNorm);
        pipelineDesc.depthAttachmentFormat = Render::RHI_Format::D32_SFloat;
        Render::PipelineHandle forwardPipeline = m_rhi.CreateGraphicsPipeline(pipelineDesc);
        m_rhi.SetDebugName(forwardPipeline, materialDebugName + ".ForwardPipeline");

        Asset::ShaderHandle gbufferFsAsset = m_assetManager.LoadShader("Assets/Shaders/gbuffer.frag.spv");
        const auto* gbufferFsSpirv = m_assetManager.GetShader(gbufferFsAsset);
        Render::ShaderHandle gbufferFs = m_rhi.CreateShader({
            .stage = Render::RHI_ShaderStage::Fragment,
            .code = gbufferFsSpirv->spirv.data(),
            .codeSize = gbufferFsSpirv->spirv.size(),
        });
        m_rhi.SetDebugName(gbufferFs, materialDebugName + ".GBufferFragmentShader");

        Render::PipelineDesc gbufferPipelineDesc = pipelineDesc;
        gbufferPipelineDesc.fragmentShader = gbufferFs;
        gbufferPipelineDesc.colorAttachmentFormats.clear();
        gbufferPipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA8_UNorm);
        gbufferPipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA16_Float);
        gbufferPipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA8_UNorm);
        Render::PipelineHandle gbufferPipeline = m_rhi.CreateGraphicsPipeline(gbufferPipelineDesc);
        m_rhi.SetDebugName(gbufferPipeline, materialDebugName + ".GBufferPipeline");

        // ubo
        std::vector<const Asset::ShaderParamDesc*> sortedParams;
        sortedParams.reserve(tmplData->parameters.size());
        for (const auto& [name, desc] : tmplData->parameters)
        {
            sortedParams.push_back(&desc);
        }

        // 按照字典序排序 保证 ubo 布局一致
        std::ranges::sort(sortedParams, [](const auto* a, const auto* b) { return a->name < b->name; });
        std::vector<uint8_t> uboData;
        uboData.reserve(256);
        size_t currentOffset = 0;

        for (const auto* param : sortedParams)
        {
            size_t align = 4;
            size_t size = 4;

            switch (param->type)
            {
            case Asset::ShaderParamTypes::Float:
                align = 4;
                size = 4;
                break;
            case Asset::ShaderParamTypes::Vec2:
                align = 8;
                size = 8;
                break;
            case Asset::ShaderParamTypes::Vec3:
                align = 16;
                size = 12;
                break;
            case Asset::ShaderParamTypes::Vec4:
                align = 16;
                size = 16;
                break;
            default:
                break;
            }

            currentOffset = (currentOffset + align - 1) & ~(align - 1);
            if (currentOffset + size > uboData.size())
            {
                uboData.resize(currentOffset + size + 64);
            }

            if (param->type == Asset::ShaderParamTypes::Float)
            {
                float val = param->defaultValues.empty() ? 0.0f : param->defaultValues[0];
                if (materialData->floatParams.contains(param->name))
                {
                    val = materialData->floatParams.at(param->name);
                }
                std::memcpy(uboData.data() + currentOffset, &val, sizeof(float));
            }
            else
            {
                std::vector<float> val = param->defaultValues;
                if (materialData->vectorParams.contains(param->name))
                {
                    val = materialData->vectorParams.at(param->name);
                }

                size_t numFloats = size / sizeof(float);
                size_t copySize = std::min(val.size(), numFloats) * sizeof(float);
                if (copySize > 0)
                {
                    std::memcpy(uboData.data() + currentOffset, val.data(), copySize);
                }
            }

            currentOffset += size;
        }

        currentOffset = (currentOffset + 15) & ~15;

        Render::BufferDesc uboDesc{
            .size = std::max<uint64_t>(currentOffset, 16),
            .usage = Render::RHI_BufferUsage::Uniform,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };

        Render::BufferHandle uboHandle = m_rhi.CreateBuffer(uboDesc);
        m_rhi.SetDebugName(uboHandle, materialDebugName + ".Parameters");
        std::memcpy(m_rhi.GetMappedData(uboHandle), uboData.data(), currentOffset);

        Render::ResourceBindingGroup bindings;
        bindings.BindBuffer(0, uboHandle, 0, currentOffset);

        for (const auto& [texName, texPath] : materialData->textureParams)
        {
            if (tmplData->textures.contains(texName))
            {
                uint32_t slot = tmplData->textures.at(texName).slot;
                Asset::TextureHandle tAsset = m_assetManager.LoadTexture(texPath);

                TextureHandle tResource = UploadTexture(tAsset);
                bindings.BindTexture(slot, GetTexture(tResource).texture);
            }
        }

        MaterialHandle handle = m_materials.Create({
            .pipeline = forwardPipeline,
            .forwardPipeline = forwardPipeline,
            .gbufferPipeline = gbufferPipeline,
            .vertexShader = vs,
            .fragmentShader = fs,
            .gbufferFragmentShader = gbufferFs,
            .uboBuffer = uboHandle,
            .bindings = bindings,
        });

        m_materialCache[assetHandle] = handle;
        return handle;
    }

    const MeshGPU& ResourceManager::GetMesh(MeshHandle handle) const
    {
        return *m_meshes.Get(handle);
    }

    const TextureGPU& ResourceManager::GetTexture(TextureHandle handle) const
    {
        return *m_textures.Get(handle);
    }

    const MaterialGPU& ResourceManager::GetMaterial(MaterialHandle handle) const
    {
        return *m_materials.Get(handle);
    }

    bool ResourceManager::Unload(MeshHandle handle)
    {
        MeshGPU* mesh = m_meshes.Get(handle);
        if (!mesh)
            return false;
        m_rhi.DestroyBuffer(mesh->vertexBuffer);
        m_rhi.DestroyBuffer(mesh->indexBuffer);
        RemoveCachedHandle(handle, m_meshCache);
        m_meshes.Destroy(handle);
        return true;
    }

    bool ResourceManager::Unload(TextureHandle handle)
    {
        TextureGPU* texture = m_textures.Get(handle);
        if (!texture)
            return false;
        m_rhi.DestroyTexture(texture->texture);
        RemoveCachedHandle(handle, m_textureCache);
        m_textures.Destroy(handle);
        return true;
    }

    bool ResourceManager::Unload(MaterialHandle handle)
    {
        MaterialGPU* material = m_materials.Get(handle);
        if (!material)
            return false;
        m_rhi.DestroyPipeline(material->forwardPipeline);
        if (material->gbufferPipeline != material->forwardPipeline)
            m_rhi.DestroyPipeline(material->gbufferPipeline);
        m_rhi.DestroyShader(material->vertexShader);
        m_rhi.DestroyShader(material->fragmentShader);
        m_rhi.DestroyShader(material->gbufferFragmentShader);
        m_rhi.DestroyBuffer(material->uboBuffer);
        RemoveCachedHandle(handle, m_materialCache);
        m_materials.Destroy(handle);
        return true;
    }

    void ResourceManager::UnloadAll()
    {
        std::vector<MeshHandle> meshes;
        std::vector<TextureHandle> textures;
        std::vector<MaterialHandle> materials;
        m_meshes.ForEach([&](MeshHandle handle, MeshGPU&) { meshes.push_back(handle); });
        m_textures.ForEach([&](TextureHandle handle, TextureGPU&) { textures.push_back(handle); });
        m_materials.ForEach([&](MaterialHandle handle, MaterialGPU&) { materials.push_back(handle); });
        for (const auto handle : meshes)
            Unload(handle);
        for (const auto handle : textures)
            Unload(handle);
        for (const auto handle : materials)
            Unload(handle);
    }

    std::vector<BufferUploadRequest> ResourceManager::GetBufferUploadJobs()
    {
        std::lock_guard lock(m_uploadMutex);
        auto copy = m_pendingBufferUploads;
        m_pendingBufferUploads.clear();
        return copy;
    }
    std::vector<TextureUploadRequest> ResourceManager::GetTextureUploadJobs()
    {
        std::lock_guard lock(m_uploadMutex);
        auto copy = m_pendingTextureUploads;
        m_pendingTextureUploads.clear();
        return copy;
    }

    Render::BufferHandle ResourceManager::UploadBoneMatrices(const std::vector<Math::Mat4>& matrices, Render::BufferHandle bufferHandle)
    {
        if (!bufferHandle.IsValid())
        {
            Render::BufferDesc boneDesc{
                .size = 128 * sizeof(Math::Mat4),
                .usage = Render::RHI_BufferUsage::Uniform,
                .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
            };
            bufferHandle = m_rhi.CreateBuffer(boneDesc);
            m_rhi.SetDebugName(bufferHandle, "Animation.BoneMatrices");
        }

        if (bufferHandle.IsValid())
        {
            auto* data = static_cast<Math::Mat4*>(m_rhi.GetMappedData(bufferHandle));
            if (data)
            {
                // NOTE: 初始化数据, 后期拷贝的时候不会出现未定义数据
                for (int i = 0; i < 128; ++i)
                {
                    data[i] = Math::Mat4::Identity();
                }
                size_t copyCount = std::min(matrices.size(), (size_t)128);
                std::vector<Math::Mat4> transposed;
                transposed.resize(copyCount);
                for (size_t i = 0; i < copyCount; ++i)
                {
                    // 转制后的结果
                    transposed[i] = matrices[i].Transposed();
                }

                std::memcpy(data, transposed.data(), copyCount * sizeof(Math::Mat4));
            }
        }

        return bufferHandle;
    }
} // namespace ChikaEngine::Resource
