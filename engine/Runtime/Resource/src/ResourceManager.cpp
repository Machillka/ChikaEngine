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
#include <string>
#include <unordered_map>

namespace ChikaEngine::Resource
{
    namespace
    {
        Render::RHI_Format ToTextureFormat(Asset::TexturePixelStorage storage, bool srgb)
        {
            switch (storage)
            {
            case Asset::TexturePixelStorage::UNorm8:
                return srgb ? Render::RHI_Format::RGBA8_SRGB : Render::RHI_Format::RGBA8_UNorm;
            case Asset::TexturePixelStorage::Float16:
                return Render::RHI_Format::RGBA16_Float;
            case Asset::TexturePixelStorage::Float32:
                return Render::RHI_Format::RGBA32_Float;
            }
            return Render::RHI_Format::Unknown;
        }

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
         * @brief 从单个 Shader Stage Reflection 构建 Depth-only 等单 Stage Pipeline 接口。
         */
        std::optional<Shader::ShaderProgramInterface> BuildSingleStageInterface(const Asset::ShaderData& shader)
        {
            if (!shader.hasReflection)
            {
                LOG_ERROR("ResourceManager", "Shader reflection sidecar is missing");
                return std::nullopt;
            }
            const std::array stages{ shader.reflection };
            Shader::ShaderProgramBuildResult result = Shader::BuildShaderProgramInterface(stages);
            for (const std::string& error : result.errors)
                LOG_ERROR("ResourceManager", "Shader interface conflict: {}", error);
            if (!result.success)
                return std::nullopt;
            return std::move(result.interface);
        }

        /**
         * @brief 过滤绑定组，只保留目标 Pipeline Reflection 实际声明的 Descriptor。
         */
        std::vector<Render::ResourceBindingGroup> FilterBindingGroups(const std::vector<Render::ResourceBindingGroup>& groups, const Shader::ShaderProgramInterface& interface)
        {
            const auto hasResource = [&](uint32_t set, uint32_t binding, Shader::ShaderDescriptorType type) { return std::ranges::any_of(interface.resources, [&](const Shader::ShaderResourceBinding& resource) { return resource.set == set && resource.binding == binding && resource.type == type; }); };
            std::vector<Render::ResourceBindingGroup> result;
            for (const Render::ResourceBindingGroup& source : groups)
            {
                Render::ResourceBindingGroup filtered{ .set = source.set, .lifetime = source.lifetime };
                std::ranges::copy_if(source.textures, std::back_inserter(filtered.textures), [&](const auto& binding) { return hasResource(source.set, binding.binding, binding.type); });
                std::ranges::copy_if(source.buffers, std::back_inserter(filtered.buffers), [&](const auto& binding) { return hasResource(source.set, binding.binding, binding.type); });
                std::ranges::copy_if(source.samplers, std::back_inserter(filtered.samplers), [&](const auto& binding) { return hasResource(source.set, binding.binding, Shader::ShaderDescriptorType::Sampler); });
                if (!filtered.textures.empty() || !filtered.buffers.empty() || !filtered.samplers.empty())
                    result.push_back(std::move(filtered));
            }
            return result;
        }

        void ReplaceMaterialParameterBuffer(std::vector<Render::ResourceBindingGroup>& groups, Render::BufferHandle sourceBuffer, Render::BufferHandle instanceBuffer, uint64_t parameterBufferSize)
        {
            for (Render::ResourceBindingGroup& group : groups)
            {
                for (Render::ResourceBindingGroup::BufferBind& binding : group.buffers)
                {
                    if (binding.buf == sourceBuffer || binding.name == "material")
                    {
                        binding.buf = instanceBuffer;
                        binding.offset = 0;
                        binding.size = parameterBufferSize;
                    }
                }
            }
        }

        /**
         * @brief Resolves a descriptor by name first, then by the reflected slot contract.
         *
         * Some SPIR-V reflection backends report storage-buffer instance names differently. The fallback
         * still validates set/binding/type against Reflection, so runtime code does not reintroduce hardcoded
         * layouts that bypass shader introspection.
         */
        Render::ResourceBindingHandle ResolveResourceBindingOrSlot(const Shader::ShaderProgramInterface& interface, std::string_view resourceName, uint32_t set, uint32_t binding, Shader::ShaderDescriptorType type)
        {
            Render::ResourceBindingHandle resolved = Render::ResolveResourceBinding(interface, resourceName);
            if (resolved.IsValid())
                return resolved;

            const auto found = std::ranges::find_if(interface.resources,
                                                    [&](const Shader::ShaderResourceBinding& resource)
                                                    {
                                                        return resource.set == set && resource.binding == binding && resource.type == type;
                                                    });
            if (found == interface.resources.end())
                return {};

            return {
                .name = found->name.empty() ? std::string(resourceName) : found->name,
                .set = found->set,
                .binding = found->binding,
                .type = found->type,
                .arrayCount = found->arrayCount,
            };
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
                .gpuVisibleInstances = ResolveResourceBindingOrSlot(interface, "gpuVisibleInstances", 2, 2, Shader::ShaderDescriptorType::StorageBuffer),
                .gpuInstances = ResolveResourceBindingOrSlot(interface, "gpuInstances", 2, 3, Shader::ShaderDescriptorType::StorageBuffer),
                .lights = Render::ResolveResourceBinding(interface, "lights"),
            };
        }

        Render::TextureDimension ToTextureDimension(Asset::TextureShape shape)
        {
            return shape == Asset::TextureShape::TextureCube ? Render::TextureDimension::TextureCube : Render::TextureDimension::Texture2D;
        }

        bool IsEnvironmentUsage(Asset::TextureAssetUsage usage)
        {
            switch (usage)
            {
            case Asset::TextureAssetUsage::Environment:
            case Asset::TextureAssetUsage::EnvironmentIrradiance:
            case Asset::TextureAssetUsage::EnvironmentPrefiltered:
            case Asset::TextureAssetUsage::EnvironmentBrdfLut:
            case Asset::TextureAssetUsage::ReflectionProbe:
                return true;
            default:
                return false;
            }
        }

        /**
         * @brief 将资产参数类型映射为运行时可编辑参数类型。
         */
        MaterialParameterType ToMaterialParameterType(Asset::ShaderParamTypes type)
        {
            switch (type)
            {
            case Asset::ShaderParamTypes::Vec2:
                return MaterialParameterType::Vec2;
            case Asset::ShaderParamTypes::Vec3:
                return MaterialParameterType::Vec3;
            case Asset::ShaderParamTypes::Vec4:
                return MaterialParameterType::Vec4;
            case Asset::ShaderParamTypes::Bool:
                return MaterialParameterType::Bool;
            case Asset::ShaderParamTypes::Float:
            default:
                return MaterialParameterType::Float;
            }
        }

        uint32_t ComponentCount(MaterialParameterType type)
        {
            switch (type)
            {
            case MaterialParameterType::Vec2:
                return 2;
            case MaterialParameterType::Vec3:
                return 3;
            case MaterialParameterType::Vec4:
                return 4;
            case MaterialParameterType::Bool:
            case MaterialParameterType::Float:
            default:
                return 1;
            }
        }

        Shader::ShaderValueType ExpectedShaderValueType(Asset::ShaderParamTypes type)
        {
            switch (type)
            {
            case Asset::ShaderParamTypes::Float:
                return Shader::ShaderValueType::Float;
            case Asset::ShaderParamTypes::Vec2:
                return Shader::ShaderValueType::Float2;
            case Asset::ShaderParamTypes::Vec3:
                return Shader::ShaderValueType::Float3;
            case Asset::ShaderParamTypes::Vec4:
                return Shader::ShaderValueType::Float4;
            default:
                return Shader::ShaderValueType::Unknown;
            }
        }

        std::vector<float> NormalizeMaterialValues(std::vector<float> values, uint32_t componentCount)
        {
            values.resize(componentCount, 0.0f);
            return values;
        }

        std::vector<float> ResolveMaterialParameterValue(const Asset::ShaderParamDesc& parameter, const Asset::MaterialData& material, uint32_t componentCount)
        {
            std::vector<float> values = parameter.defaultValues;
            if (parameter.type == Asset::ShaderParamTypes::Float && material.floatParams.contains(parameter.name))
                values = { material.floatParams.at(parameter.name) };
            else if (material.vectorParams.contains(parameter.name))
                values = material.vectorParams.at(parameter.name);
            return NormalizeMaterialValues(std::move(values), componentCount);
        }

        bool WriteMaterialParameterValue(std::vector<uint8_t>& data, const MaterialParameterRuntime& runtime, std::string_view name, std::span<const float> values)
        {
            if (values.size() != runtime.componentCount)
            {
                LOG_ERROR("ResourceManager", "Material parameter '{}' expected {} components but got {}", name, runtime.componentCount, values.size());
                return false;
            }

            const size_t copySize = values.size_bytes();
            if (copySize > runtime.size || runtime.offset + copySize > data.size())
            {
                LOG_ERROR("ResourceManager", "Material parameter '{}' write exceeds reflected buffer range", name);
                return false;
            }

            std::memcpy(data.data() + runtime.offset, values.data(), copySize);
            return true;
        }

        std::optional<MaterialParameterRuntime> BuildMaterialParameterRuntime(const Shader::ShaderBufferMember& member, const Asset::ShaderParamDesc& parameter, const Asset::MaterialData& material)
        {
            const Shader::ShaderValueType expectedType = ExpectedShaderValueType(parameter.type);
            if (member.type != expectedType)
            {
                LOG_ERROR("ResourceManager", "Material parameter '{}' type does not match reflected Shader member", parameter.name);
                return std::nullopt;
            }

            const MaterialParameterType runtimeType = ToMaterialParameterType(parameter.type);
            const uint32_t componentCount = ComponentCount(runtimeType);
            MaterialParameterRuntime runtime{
                .type = runtimeType,
                .componentCount = componentCount,
                .offset = member.offset,
                .size = member.size,
                .value = ResolveMaterialParameterValue(parameter, material, componentCount),
                .defaultValue = NormalizeMaterialValues(parameter.defaultValues, componentCount),
            };
            return runtime;
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
        {
            m_textureUploadStatuses[assetHandle] = TextureUploadStatus::Ready;
            return m_textureCache[assetHandle];
        }
        return _UploadTexture(assetHandle);
    }

    MaterialHandle ResourceManager::UploadMaterial(Asset::MaterialHandle assetHandle)
    {
        if (m_materialCache.contains(assetHandle))
            return m_materialCache[assetHandle];
        return _UploadMaterial(assetHandle);
    }

    MaterialHandle ResourceManager::CloneMaterial(MaterialHandle sourceHandle)
    {
        const MaterialGPU* source = m_materials.Get(sourceHandle);
        if (!source)
            return MaterialHandle::Invalid();

        MaterialGPU instance = *source;
        const uint64_t parameterBufferSize = std::max<uint64_t>(source->parameterBufferSize, 16);
        instance.uboBuffer = m_rhi.CreateBuffer({
            .size = parameterBufferSize,
            .usage = Render::RHI_BufferUsage::Uniform,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        });
        if (!instance.uboBuffer.IsValid())
            return MaterialHandle::Invalid();

        m_rhi.SetDebugName(instance.uboBuffer, "Material.RuntimeInstance.Parameters");
        if (void* mapped = m_rhi.GetMappedData(instance.uboBuffer))
            std::memcpy(mapped, source->parameterData.data(), source->parameterData.size());
        else
        {
            m_rhi.DestroyBuffer(instance.uboBuffer);
            return MaterialHandle::Invalid();
        }

        ReplaceMaterialParameterBuffer(instance.bindings, source->uboBuffer, instance.uboBuffer, source->parameterBufferSize);
        ReplaceMaterialParameterBuffer(instance.shadowBindings, source->uboBuffer, instance.uboBuffer, source->parameterBufferSize);
        instance.ownsPipelineResources = false;
        instance.runtimeInstance = true;
        return m_materials.Create(std::move(instance));
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
            m_pendingBufferUploads.push_back({ stagingV, vbo, vSize, Render::ResourceState::VertexBuffer });
            m_pendingBufferUploads.push_back({ stagingI, ibo, iSize, Render::ResourceState::IndexBuffer });
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
        {
            m_textureUploadStatuses[assetHandle] = TextureUploadStatus::MissingAsset;
            return TextureHandle::Invalid();
        }

        m_textureUploadStatuses[assetHandle] = TextureUploadStatus::InvalidPayload;

        const uint64_t imageSize = data->pixels.size();
        if (data->channels != 4 || imageSize == 0 || !Asset::IsTexturePayloadLayoutValid(*data))
        {
            LOG_ERROR("ResourceManager", "Texture '{}' has an invalid CPU payload layout (storage={}, rowBytes={}, layerBytes={}, totalBytes={})", data->path, Asset::TexturePixelStorageName(data->pixelStorage), data->rowBytes, data->layerBytes, imageSize);
            return TextureHandle::Invalid();
        }
        if (data->pixelStorage != Asset::TexturePixelStorage::UNorm8 && data->srgb)
        {
            LOG_ERROR("ResourceManager", "Texture '{}' is floating-point but requests sRGB sampling", data->path);
            return TextureHandle::Invalid();
        }
        if (data->generateMips || data->mipLevels != 1)
        {
            LOG_ERROR("ResourceManager", "Texture '{}' requests mip data that is not present in the upload payload", data->path);
            return TextureHandle::Invalid();
        }

        const Render::TextureDimension dimension = ToTextureDimension(data->shape);
        const Render::RHI_Format format = ToTextureFormat(data->pixelStorage, data->srgb);
        const Render::RHICapabilities& capabilities = m_rhi.GetCapabilities();
        const uint32_t maximumDimension = dimension == Render::TextureDimension::TextureCube ? capabilities.maxTextureCubeSize : capabilities.maxTexture2DSize;
        if (maximumDimension != 0 && (data->width > maximumDimension || data->height > maximumDimension))
        {
            m_textureUploadStatuses[assetHandle] = TextureUploadStatus::DimensionLimitExceeded;
            LOG_ERROR("ResourceManager", "Texture '{}' size {}x{} exceeds the RHI {} limit {}", data->path, data->width, data->height, dimension == Render::TextureDimension::TextureCube ? "Cubemap" : "2D texture", maximumDimension);
            return TextureHandle::Invalid();
        }
        Render::TextureDesc desc{
            .width = data->width,
            .height = data->height,
            .format = format,
            .mipLevels = std::max(1u, data->mipLevels),
            .arrayLayers = std::max(1u, data->arrayLayers),
            .usage = Render::RHI_TextureUsage::Sampled,
            .dimension = dimension,
        };
        if (!Render::IsTextureDescValid(desc))
        {
            LOG_ERROR("ResourceManager", "Texture '{}' has invalid RHI description", data->path);
            return TextureHandle::Invalid();
        }
        const uint64_t expectedRowBytes = static_cast<uint64_t>(data->width) * Render::RHIFormatBytesPerTexel(desc.format);
        const uint64_t expectedLayerBytes = expectedRowBytes * data->height;
        const uint64_t expectedImageSize = expectedLayerBytes * desc.arrayLayers;
        if (Render::RHIFormatBytesPerTexel(desc.format) == 0 || data->rowBytes != expectedRowBytes || data->layerBytes != expectedLayerBytes || imageSize != expectedImageSize)
        {
            LOG_ERROR("ResourceManager", "Texture '{}' payload does not match RHI format {} (row={} expectedRow={}, layer={} expectedLayer={}, total={} expectedTotal={})", data->path, static_cast<uint32_t>(desc.format), data->rowBytes, expectedRowBytes, data->layerBytes, expectedLayerBytes, imageSize, expectedImageSize);
            return TextureHandle::Invalid();
        }
        m_textureUploadStatuses[assetHandle] = TextureUploadStatus::GPUUploadFailed;
        Render::TextureHandle tex = m_rhi.CreateTexture(desc);
        if (!tex.IsValid())
            return TextureHandle::Invalid();
        const std::string textureDebugName = data->path.empty() ? "Texture.Unnamed" : "Texture." + data->path;
        m_rhi.SetDebugName(tex, textureDebugName);

        const Render::TextureViewDesc viewDesc{
            .texture = tex,
            .range = { .baseMipLevel = 0, .mipLevelCount = desc.mipLevels, .baseArrayLayer = 0, .arrayLayerCount = desc.arrayLayers },
            .dimension = dimension,
        };
        Render::TextureViewHandle defaultView = m_rhi.CreateTextureView(viewDesc);
        if (!defaultView.IsValid())
        {
            m_rhi.DestroyTexture(tex);
            return TextureHandle::Invalid();
        }
        m_rhi.SetDebugName(defaultView, textureDebugName + ".DefaultView");

        const bool clampSampler = dimension == Render::TextureDimension::TextureCube || IsEnvironmentUsage(data->usage);
        Render::SamplerHandle sampler = m_rhi.CreateSampler({
            .minFilter = Render::FilterMode::Linear,
            .magFilter = Render::FilterMode::Linear,
            .addressU = clampSampler ? Render::AddressMode::ClampToEdge : Render::AddressMode::Repeat,
            .addressV = clampSampler ? Render::AddressMode::ClampToEdge : Render::AddressMode::Repeat,
            .addressW = clampSampler ? Render::AddressMode::ClampToEdge : Render::AddressMode::Repeat,
        });
        if (!sampler.IsValid())
        {
            m_rhi.DestroyTextureView(defaultView);
            m_rhi.DestroyTexture(tex);
            return TextureHandle::Invalid();
        }
        m_rhi.SetDebugName(sampler, textureDebugName + ".Sampler");

        std::vector<Render::TextureViewHandle> mipViews;
        std::vector<Render::TextureViewHandle> faceViews;
        auto destroyTextureContractHandles = [&]()
        {
            for (Render::TextureViewHandle view : mipViews)
            {
                if (view.IsValid())
                    m_rhi.DestroyTextureView(view);
            }
            for (Render::TextureViewHandle view : faceViews)
            {
                if (view.IsValid())
                    m_rhi.DestroyTextureView(view);
            }
            m_rhi.DestroySampler(sampler);
            m_rhi.DestroyTextureView(defaultView);
            m_rhi.DestroyTexture(tex);
        };

        if ((dimension == Render::TextureDimension::TextureCube || IsEnvironmentUsage(data->usage)) && desc.mipLevels > 1)
        {
            mipViews.reserve(desc.mipLevels);
            for (uint32_t mip = 0; mip < desc.mipLevels; ++mip)
            {
                Render::TextureViewHandle mipView = m_rhi.CreateTextureView({
                    .texture = tex,
                    .range = { .baseMipLevel = mip, .mipLevelCount = 1, .baseArrayLayer = 0, .arrayLayerCount = desc.arrayLayers },
                    .dimension = dimension,
                });
                if (!mipView.IsValid())
                {
                    destroyTextureContractHandles();
                    return TextureHandle::Invalid();
                }
                m_rhi.SetDebugName(mipView, textureDebugName + ".MipView." + std::to_string(mip));
                mipViews.push_back(mipView);
            }
        }

        if (dimension == Render::TextureDimension::TextureCube)
        {
            faceViews.reserve(desc.arrayLayers);
            for (uint32_t face = 0; face < desc.arrayLayers; ++face)
            {
                Render::TextureViewHandle faceView = m_rhi.CreateTextureView({
                    .texture = tex,
                    .range = { .baseMipLevel = 0, .mipLevelCount = desc.mipLevels, .baseArrayLayer = face, .arrayLayerCount = 1 },
                    .dimension = Render::TextureDimension::Texture2D,
                });
                if (!faceView.IsValid())
                {
                    destroyTextureContractHandles();
                    return TextureHandle::Invalid();
                }
                m_rhi.SetDebugName(faceView, textureDebugName + ".FaceView." + std::to_string(face));
                faceViews.push_back(faceView);
            }
        }

        Render::BufferDesc stagingDesc{
            .size = imageSize,
            .usage = Render::RHI_BufferUsage::Staging,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };
        Render::BufferHandle staging = m_rhi.CreateBuffer(stagingDesc);
        if (!staging.IsValid())
        {
            LOG_ERROR("ResourceManager", "Texture '{}' failed to allocate a {}-byte staging buffer", data->path, imageSize);
            destroyTextureContractHandles();
            return TextureHandle::Invalid();
        }
        m_rhi.SetDebugName(staging, textureDebugName + ".Staging");
        void* mappedData = m_rhi.GetMappedData(staging);
        if (!mappedData)
        {
            LOG_ERROR("ResourceManager", "Texture '{}' staging buffer is not mapped", data->path);
            m_rhi.DestroyBuffer(staging);
            destroyTextureContractHandles();
            return TextureHandle::Invalid();
        }
        std::memcpy(mappedData, data->pixels.data(), imageSize);

        {
            std::lock_guard<std::mutex> lock(m_uploadMutex);
            m_pendingTextureUploads.push_back({
                .staging = staging,
                .dst = tex,
                .width = data->width,
                .height = data->height,
                .mipLevels = desc.mipLevels,
                .arrayLayers = desc.arrayLayers,
                .size = imageSize,
                .rowBytes = data->rowBytes,
                .layerBytes = data->layerBytes,
                .format = desc.format,
                .dimension = desc.dimension,
            });
        }

        LOG_INFO("ResourceManager", "Texture upload queued path='{}' source={} size={}x{} storage={} rhiFormat={} layers={} mips={} rowBytes={} layerBytes={} stagingBytes={}", data->path, Asset::TextureSourceEncodingName(data->sourceEncoding), data->width, data->height, Asset::TexturePixelStorageName(data->pixelStorage), static_cast<uint32_t>(desc.format), desc.arrayLayers, desc.mipLevels, data->rowBytes, data->layerBytes, imageSize);

        // SubmitImmediate(
        //     [&](Render::IRHICommandList* cmd)
        //     {
        //         cmd->InsertTextureBarrier(tex, Render::ResourceState::Undefined, Render::ResourceState::TransferDst);
        //         cmd->CopyBufferToTexture(staging, tex, data->width, data->height, desc.arrayLayers);
        //         cmd->InsertTextureBarrier(tex, Render::ResourceState::TransferDst, Render::ResourceState::ShaderResource);
        //     });

        m_rhi.DestroyBuffer(staging);

        TextureHandle handle = m_textures.Create({
            .texture = tex,
            .defaultView = defaultView,
            .sampler = sampler,
            .mipViews = std::move(mipViews),
            .faceViews = std::move(faceViews),
            .dimension = dimension,
            .format = desc.format,
            .usage = data->usage,
            .width = data->width,
            .height = data->height,
            .mipLevels = desc.mipLevels,
            .arrayLayers = desc.arrayLayers,
        });
        m_textureCache[assetHandle] = handle;
        m_textureUploadStatuses[assetHandle] = TextureUploadStatus::Ready;
        return handle;
    }

    MaterialHandle ResourceManager::_UploadMaterial(Asset::MaterialHandle assetHandle)
    {
        const Asset::MaterialData* materialData = m_assetManager.GetMaterial(assetHandle);
        if (!materialData)
            return MaterialHandle::Invalid();

        Asset::ShaderTemplateHandle tmplHandle = m_assetManager.LoadShaderTemplate(materialData->shaderTemplate);
        const Asset::ShaderTemplateData* tmplData = m_assetManager.GetShaderTemplate(tmplHandle);
        if (!tmplData)
            return MaterialHandle::Invalid();

        Asset::ShaderHandle vsAsset = m_assetManager.LoadShader(tmplData->vertexShader);
        Asset::ShaderHandle fsAsset = m_assetManager.LoadShader(tmplData->fragmentShader);
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
        // Forward Opaque/Transparent 都在线性 HDR Scene Color 中执行，显示转换由 Post Process 完成。
        pipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA16_Float);
        pipelineDesc.depthAttachmentFormat = Render::RHI_Format::D32_SFloat;
        Render::PipelineHandle forwardPipeline = m_rhi.CreateGraphicsPipeline(pipelineDesc);
        m_rhi.SetDebugName(forwardPipeline, materialDebugName + ".ForwardPipeline");

        const auto shadowInterface = BuildSingleStageInterface(*vsSpirv);
        if (!shadowInterface)
            return MaterialHandle::Invalid();
        Render::PipelineDesc shadowPipelineDesc = pipelineDesc;
        shadowPipelineDesc.fragmentShader = {};
        shadowPipelineDesc.shaderInterface = *shadowInterface;
        shadowPipelineDesc.colorAttachmentFormats.clear();
        shadowPipelineDesc.alphaBlendEnable = false;
        Render::PipelineHandle shadowPipeline = m_rhi.CreateGraphicsPipeline(shadowPipelineDesc);
        m_rhi.SetDebugName(shadowPipeline, materialDebugName + ".ShadowPipeline");

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
        gbufferPipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA16_Float);
        gbufferPipelineDesc.colorAttachmentFormats.push_back(Render::RHI_Format::RGBA16_Float);
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
        std::unordered_map<std::string, MaterialParameterRuntime> materialParameters;
        for (const auto& [name, parameter] : tmplData->parameters)
        {
            const Shader::ShaderBufferMember* member = forwardInterface->FindBufferMember("material", name);
            if (!member)
            {
                LOG_ERROR("ResourceManager", "Material parameter '{}' is not present in reflected buffer 'material'", name);
                continue;
            }
            std::optional<MaterialParameterRuntime> runtime = BuildMaterialParameterRuntime(*member, parameter, *materialData);
            if (!runtime)
                continue;
            if (!WriteMaterialParameterValue(uboData, *runtime, name, runtime->value))
                continue;
            materialParameters.emplace(name, std::move(*runtime));
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

        for (const auto& [texName, textureReference] : materialData->textureParams)
        {
            if (tmplData->textures.contains(texName))
            {
                Asset::TextureHandle tAsset = m_assetManager.LoadTexture(textureReference);

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
            .shadowPipeline = shadowPipeline,
            .vertexShader = vs,
            .fragmentShader = fs,
            .gbufferFragmentShader = gbufferFs,
            .uboBuffer = uboHandle,
            .parameterBufferSize = materialResource->buffer.size,
            .parameterData = std::move(uboData),
            .parameters = std::move(materialParameters),
            .forwardDrawBindings = ResolveMaterialDrawBindings(*forwardInterface),
            .gbufferDrawBindings = ResolveMaterialDrawBindings(*gbufferInterface),
            .shadowDrawBindings = ResolveMaterialDrawBindings(*shadowInterface),
            .bindings = bindings,
            .shadowBindings = FilterBindingGroups(bindings, *shadowInterface),
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

    const MeshGPU* ResourceManager::TryGetMesh(MeshHandle handle) const
    {
        return m_meshes.Get(handle);
    }

    const TextureGPU* ResourceManager::TryGetTexture(TextureHandle handle) const
    {
        return m_textures.Get(handle);
    }

    TextureUploadStatus ResourceManager::GetTextureUploadStatus(Asset::TextureHandle handle) const
    {
        const auto status = m_textureUploadStatuses.find(handle);
        return status == m_textureUploadStatuses.end() ? TextureUploadStatus::Unknown : status->second;
    }

    const MaterialGPU* ResourceManager::TryGetMaterial(MaterialHandle handle) const
    {
        return m_materials.Get(handle);
    }

    std::vector<MaterialParameterInfo> ResourceManager::GetMaterialParameters(MaterialHandle handle) const
    {
        const MaterialGPU* material = m_materials.Get(handle);
        if (!material)
            return {};

        std::vector<MaterialParameterInfo> result;
        result.reserve(material->parameters.size());
        for (const auto& [name, runtime] : material->parameters)
        {
            result.push_back({
                .name = name,
                .type = runtime.type,
                .componentCount = runtime.componentCount,
                .value = runtime.value,
                .defaultValue = runtime.defaultValue,
            });
        }
        std::ranges::sort(result, {}, &MaterialParameterInfo::name);
        return result;
    }

    const MaterialParameterRuntime* ResourceManager::FindMaterialParameter(MaterialHandle handle, std::string_view name) const
    {
        const MaterialGPU* material = m_materials.Get(handle);
        if (!material)
            return nullptr;
        const auto found = material->parameters.find(std::string(name));
        return found == material->parameters.end() ? nullptr : &found->second;
    }

    bool ResourceManager::UpdateMaterialParameter(MaterialHandle handle, std::string_view name, std::span<const float> value)
    {
        MaterialGPU* material = m_materials.Get(handle);
        if (!material)
        {
            LOG_ERROR("ResourceManager", "Attempted to update an invalid material handle");
            return false;
        }

        const auto found = material->parameters.find(std::string(name));
        if (found == material->parameters.end())
        {
            LOG_ERROR("ResourceManager", "Material parameter '{}' does not exist", name);
            return false;
        }

        MaterialParameterRuntime& runtime = found->second;
        std::vector<uint8_t> updatedData = material->parameterData;
        if (!WriteMaterialParameterValue(updatedData, runtime, name, value))
            return false;

        void* mapped = m_rhi.GetMappedData(material->uboBuffer);
        if (!mapped)
        {
            LOG_ERROR("ResourceManager", "Material parameter '{}' cannot be updated because the UBO is not CPU mapped", name);
            return false;
        }

        const size_t copySize = value.size_bytes();
        std::memcpy(static_cast<uint8_t*>(mapped) + runtime.offset, value.data(), copySize);
        material->parameterData = std::move(updatedData);
        runtime.value.assign(value.begin(), value.end());
        return true;
    }

    bool ResourceManager::UpdateMaterialParameter(MaterialHandle handle, std::string_view name, const MaterialParameterValue& value)
    {
        const MaterialParameterRuntime* runtime = FindMaterialParameter(handle, name);
        if (!runtime)
        {
            LOG_ERROR("ResourceManager", "Material parameter '{}' does not exist", name);
            return false;
        }
        if (runtime->type != value.type)
        {
            LOG_ERROR("ResourceManager", "Material parameter '{}' type does not match", name);
            return false;
        }
        return UpdateMaterialParameter(handle, name, std::span<const float>(value.value.data(), value.value.size()));
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
        for (Render::TextureViewHandle view : texture->mipViews)
        {
            if (view.IsValid())
                m_rhi.DestroyTextureView(view);
        }
        for (Render::TextureViewHandle view : texture->faceViews)
        {
            if (view.IsValid())
                m_rhi.DestroyTextureView(view);
        }
        if (texture->defaultView.IsValid())
            m_rhi.DestroyTextureView(texture->defaultView);
        if (texture->sampler.IsValid())
            m_rhi.DestroySampler(texture->sampler);
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
        if (material->ownsPipelineResources)
        {
            m_rhi.DestroyPipeline(material->forwardPipeline);
            if (material->gbufferPipeline != material->forwardPipeline)
                m_rhi.DestroyPipeline(material->gbufferPipeline);
            if (material->shadowPipeline != material->forwardPipeline && material->shadowPipeline != material->gbufferPipeline)
                m_rhi.DestroyPipeline(material->shadowPipeline);
            m_rhi.DestroyShader(material->vertexShader);
            m_rhi.DestroyShader(material->fragmentShader);
            m_rhi.DestroyShader(material->gbufferFragmentShader);
        }
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
        m_textureUploadStatuses.clear();
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
