#include "ChikaEngine/ResourceManager.hpp"
#include "ChikaEngine/AssetHandle.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
#include "ChikaEngine/shader/ShaderInterface.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <span>

namespace ChikaEngine::Resource
{
    namespace
    {
        /**
         * @brief 把反射 Vertex Input 映射到引擎唯一的 VertexData 流布局。
         *
         * Reflection 决定 Pipeline 实际声明哪些 location；CPU VertexData 只负责提供对应 offset。
         */
        Render::VertexLayout BuildReflectedVertexLayout(const Shader::ShaderProgramInterface& interface)
        {
            Render::VertexLayout layout{
                .stride = interface.vertexInputs.empty() ? 0u : static_cast<uint32_t>(sizeof(Asset::VertexData)),
            };
            for (const auto& input : interface.vertexInputs)
            {
                Shader::ShaderValueType expectedType = Shader::ShaderValueType::Unknown;
                Render::VertexAttribute attribute{};
                switch (input.location)
                {
                case 0:
                    expectedType = Shader::ShaderValueType::Float3;
                    attribute = { input.location, Render::RHI_Format::RGB32_Float, offsetof(Asset::VertexData, position) };
                    break;
                case 1:
                    expectedType = Shader::ShaderValueType::Float3;
                    attribute = { input.location, Render::RHI_Format::RGB32_Float, offsetof(Asset::VertexData, normal) };
                    break;
                case 2:
                    expectedType = Shader::ShaderValueType::Float2;
                    attribute = { input.location, Render::RHI_Format::RG32_Float, offsetof(Asset::VertexData, uv) };
                    break;
                case 3:
                    expectedType = Shader::ShaderValueType::Int4;
                    attribute = { input.location, Render::RHI_Format::RGBA32_SInt, offsetof(Asset::VertexData, boneIndices) };
                    break;
                case 4:
                    expectedType = Shader::ShaderValueType::Float4;
                    attribute = { input.location, Render::RHI_Format::RGBA32_Float, offsetof(Asset::VertexData, boneWeights) };
                    break;
                default:
                    LOG_ERROR("ResourceManager", "Vertex input '{}' uses unsupported location {}", input.name, input.location);
                    continue;
                }
                if (input.type != expectedType)
                {
                    LOG_ERROR("ResourceManager", "Vertex input '{}' type does not match VertexData location {}", input.name, input.location);
                    continue;
                }
                layout.attributes.push_back(attribute);
            }
            return layout;
        }

        /**
         * @brief 合并 Vertex/Fragment Reflection，并输出可定位的 Pipeline 接口冲突。
         */
        std::optional<Shader::ShaderProgramInterface> BuildProgramInterface(const Asset::ShaderData& vertex, const Asset::ShaderData& fragment)
        {
            if (!vertex.hasReflection || !fragment.hasReflection)
            {
                LOG_ERROR("ResourceManager", "Shader reflection sidecar is missing");
                return std::nullopt;
            }
            const std::array stages{ vertex.reflection, fragment.reflection };
            Shader::ShaderProgramBuildResult result = Shader::BuildShaderProgramInterface(stages);
            for (const std::string& error : result.errors)
                LOG_ERROR("ResourceManager", "Shader interface conflict: {}", error);
            if (!result.success)
                return std::nullopt;
            return std::move(result.interface);
        }

        /**
         * @brief 在材质创建阶段解析逐 Draw 动态资源，避免 Renderer 热路径查询 Reflection 名称。
         */
        MaterialDrawBindings ResolveMaterialDrawBindings(const Shader::ShaderProgramInterface& interface)
        {
            return {
                .scene = Render::ResolveResourceBinding(interface, "scene"),
                .shadowMap = Render::ResolveResourceBinding(interface, "shadowMap"),
                .bones = Render::ResolveResourceBinding(interface, "uboBones"),
                .instances = Render::ResolveResourceBinding(interface, "instances"),
            };
        }

        /**
         * @brief 将 Material 参数值写入 Reflection 指定的真实 Buffer Offset。
         */
        void WriteMaterialParameter(std::vector<uint8_t>& data, const Shader::ShaderBufferMember& member, const Asset::ShaderParamDesc& parameter, const Asset::MaterialData& material)
        {
            Shader::ShaderValueType expectedType = Shader::ShaderValueType::Unknown;
            switch (parameter.type)
            {
            case Asset::ShaderParamTypes::Float:
                expectedType = Shader::ShaderValueType::Float;
                break;
            case Asset::ShaderParamTypes::Vec2:
                expectedType = Shader::ShaderValueType::Float2;
                break;
            case Asset::ShaderParamTypes::Vec3:
                expectedType = Shader::ShaderValueType::Float3;
                break;
            case Asset::ShaderParamTypes::Vec4:
                expectedType = Shader::ShaderValueType::Float4;
                break;
            default:
                break;
            }
            if (member.type != expectedType)
            {
                LOG_ERROR("ResourceManager", "Material parameter '{}' type does not match reflected Shader member", parameter.name);
                return;
            }

            std::vector<float> values = parameter.defaultValues;
            if (parameter.type == Asset::ShaderParamTypes::Float && material.floatParams.contains(parameter.name))
                values = { material.floatParams.at(parameter.name) };
            else if (material.vectorParams.contains(parameter.name))
                values = material.vectorParams.at(parameter.name);

            const size_t copySize = std::min<size_t>(member.size, values.size() * sizeof(float));
            if (member.offset + copySize <= data.size() && copySize > 0)
                std::memcpy(data.data() + member.offset, values.data(), copySize);
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
            .bounds = data->bounds,
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
        if (!tmplData)
            return MaterialHandle::Invalid();

        Asset::ShaderHandle vsAsset = m_assetManager.LoadShader(tmplData->vertexShaderPath);
        Asset::ShaderHandle fsAsset = m_assetManager.LoadShader(tmplData->fragmentShaderPath);
        const auto* vsSpirv = m_assetManager.GetShader(vsAsset);
        const auto* fsSpirv = m_assetManager.GetShader(fsAsset);
        if (!vsSpirv || !fsSpirv)
            return MaterialHandle::Invalid();

        const auto forwardInterface = BuildProgramInterface(*vsSpirv, *fsSpirv);
        if (!forwardInterface)
            return MaterialHandle::Invalid();

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

        const bool transparent = materialData->variants.contains("transparent") && materialData->variants.at("transparent");
        const bool masked = materialData->variants.contains("masked") && materialData->variants.at("masked");
        Render::PipelineDesc pipelineDesc{
            .vertexShader = vs,
            .fragmentShader = fs,
            .shaderInterface = *forwardInterface,
            .vertexLayout = BuildReflectedVertexLayout(*forwardInterface),
            .depthTest = true,
            .depthWrite = !transparent,
            .alphaBlendEnable = transparent,
        };
        pipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::BGRA8_UNorm);
        pipelineDesc.depthAttachmentFormat = Render::RHI_Format::D32_SFloat;
        Render::PipelineHandle forwardPipeline = m_rhi.CreateGraphicsPipeline(pipelineDesc);
        m_rhi.SetDebugName(forwardPipeline, materialDebugName + ".ForwardPipeline");

        Asset::ShaderHandle gbufferFsAsset = m_assetManager.LoadShader("Assets/Shaders/gbuffer.frag");
        const auto* gbufferFsSpirv = m_assetManager.GetShader(gbufferFsAsset);
        if (!gbufferFsSpirv)
            return MaterialHandle::Invalid();
        const auto gbufferInterface = BuildProgramInterface(*vsSpirv, *gbufferFsSpirv);
        if (!gbufferInterface)
            return MaterialHandle::Invalid();

        Render::ShaderHandle gbufferFs = m_rhi.CreateShader({
            .stage = Render::RHI_ShaderStage::Fragment,
            .code = gbufferFsSpirv->spirv.data(),
            .codeSize = gbufferFsSpirv->spirv.size(),
        });
        m_rhi.SetDebugName(gbufferFs, materialDebugName + ".GBufferFragmentShader");

        Render::PipelineDesc gbufferPipelineDesc = pipelineDesc;
        gbufferPipelineDesc.fragmentShader = gbufferFs;
        gbufferPipelineDesc.shaderInterface = *gbufferInterface;
        gbufferPipelineDesc.vertexLayout = BuildReflectedVertexLayout(*gbufferInterface);
        gbufferPipelineDesc.colorAttachmentFormats.clear();
        gbufferPipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA8_UNorm);
        gbufferPipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA16_Float);
        gbufferPipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA8_UNorm);
        Render::PipelineHandle gbufferPipeline = m_rhi.CreateGraphicsPipeline(gbufferPipelineDesc);
        m_rhi.SetDebugName(gbufferPipeline, materialDebugName + ".GBufferPipeline");

        // Material UBO 的大小和成员偏移来自 Reflection，不再按参数名称重新计算布局。
        const Shader::ShaderResourceBinding* materialResource = forwardInterface->FindResource("material");
        if (!materialResource || materialResource->type != Shader::ShaderDescriptorType::UniformBuffer)
        {
            LOG_ERROR("ResourceManager", "Material '{}' shader does not expose reflected uniform buffer 'material'", materialData->name);
            return MaterialHandle::Invalid();
        }
        std::vector<uint8_t> uboData(materialResource->buffer.size, 0);
        for (const auto& [name, parameter] : tmplData->parameters)
        {
            const Shader::ShaderBufferMember* member = forwardInterface->FindBufferMember("material", name);
            if (!member)
            {
                LOG_ERROR("ResourceManager", "Material parameter '{}' is not present in reflected buffer 'material'", name);
                continue;
            }
            WriteMaterialParameter(uboData, *member, parameter, *materialData);
        }

        Render::BufferDesc uboDesc{
            .size = std::max<uint64_t>(materialResource->buffer.size, 16),
            .usage = Render::RHI_BufferUsage::Uniform,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };

        Render::BufferHandle uboHandle = m_rhi.CreateBuffer(uboDesc);
        m_rhi.SetDebugName(uboHandle, materialDebugName + ".Parameters");
        std::memcpy(m_rhi.GetMappedData(uboHandle), uboData.data(), uboData.size());

        std::vector<Render::ResourceBindingGroup> bindings;
        const Render::ResourceBindingHandle materialBinding = Render::ResolveResourceBinding(*forwardInterface, "material");
        Render::BindBuffer(bindings, materialBinding, uboHandle, 0, materialResource->buffer.size, Render::ResourceBindingLifetime::Persistent);

        for (const auto& [texName, texPath] : materialData->textureParams)
        {
            if (tmplData->textures.contains(texName))
            {
                Asset::TextureHandle tAsset = m_assetManager.LoadTexture(texPath);

                TextureHandle tResource = UploadTexture(tAsset);
                const Render::ResourceBindingHandle textureBinding = Render::ResolveResourceBinding(*forwardInterface, texName);
                if (!Render::BindTexture(bindings, textureBinding, GetTexture(tResource).texture, Render::ResourceBindingLifetime::Persistent))
                    LOG_ERROR("ResourceManager", "Material texture '{}' is not present in reflected shader interface", texName);
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
            .forwardDrawBindings = ResolveMaterialDrawBindings(*forwardInterface),
            .gbufferDrawBindings = ResolveMaterialDrawBindings(*gbufferInterface),
            .bindings = bindings,
            .transparent = transparent,
            .masked = masked,
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

} // namespace ChikaEngine::Resource
