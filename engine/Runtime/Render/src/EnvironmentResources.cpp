#include "ChikaEngine/EnvironmentResources.hpp"

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderGraphBlackboard.hpp"
#include "ChikaEngine/RenderSettings.hpp"
#include "ChikaEngine/ResourceLayout.hpp"
#include "ChikaEngine/ResourceManager.hpp"

namespace ChikaEngine::Render
{
    namespace
    {
        bool IsReferenceConfigured(const Asset::AssetReference& reference)
        {
            return reference.IsValid() || !reference.diagnosticPath.empty();
        }

        TextureDesc BuildTextureDesc(const Resource::TextureGPU& texture)
        {
            return {
                .width = texture.width,
                .height = texture.height,
                .format = texture.format,
                .mipLevels = texture.mipLevels,
                .arrayLayers = texture.arrayLayers,
                .usage = RHI_TextureUsage::Sampled,
                .dimension = texture.dimension,
            };
        }
    } // namespace

    std::string_view EnvironmentResourceStatusName(EnvironmentResourceStatus status)
    {
        switch (status)
        {
        case EnvironmentResourceStatus::Disabled:
            return "disabled";
        case EnvironmentResourceStatus::MissingReference:
            return "missing-reference";
        case EnvironmentResourceStatus::AssetLoadFailed:
            return "asset-load-failed";
        case EnvironmentResourceStatus::ResourceUploadFailed:
            return "resource-upload-failed";
        case EnvironmentResourceStatus::InvalidTextureContract:
            return "invalid-texture-contract";
        case EnvironmentResourceStatus::Ready:
            return "ready";
        }
        return "unknown";
    }

    bool EnvironmentTextureResource::IsValid() const
    {
        return resource.IsValid() && texture.IsValid() && defaultView.IsValid() && sampler.IsValid() && desc.dimension == TextureDimension::TextureCube && IsTextureDescValid(desc);
    }

    bool EnvironmentResourceResolver::HasSameReference(const Asset::AssetReference& reference) const
    {
        return m_reference.guid == reference.guid && m_reference.subAsset == reference.subAsset && m_reference.expectedType == reference.expectedType && m_reference.diagnosticPath == reference.diagnosticPath;
    }

    EnvironmentResourceStatus EnvironmentResourceResolver::Update(const EnvironmentSettings& settings, Asset::AssetManager& assets, Resource::ResourceManager& resources)
    {
        if (!settings.enabled)
        {
            m_skybox = {};
            m_status = EnvironmentResourceStatus::Disabled;
            m_wasEnabled = false;
            return m_status;
        }

        const bool referenceChanged = !HasSameReference(settings.skybox);
        if (!m_wasEnabled || referenceChanged)
        {
            m_reference = settings.skybox;
            m_asset = Asset::TextureHandle::Invalid();
            m_resource = Resource::TextureHandle::Invalid();
            m_skybox = {};
            m_attemptedAssetLoad = false;
        }
        m_wasEnabled = true;

        if (!IsReferenceConfigured(m_reference))
        {
            m_status = EnvironmentResourceStatus::MissingReference;
            return m_status;
        }

        if (!m_attemptedAssetLoad)
        {
            m_asset = assets.LoadTexture(m_reference);
            m_attemptedAssetLoad = true;
        }
        if (!m_asset.IsValid())
        {
            m_status = EnvironmentResourceStatus::AssetLoadFailed;
            return m_status;
        }

        const Resource::TextureGPU* texture = resources.TryGetTexture(m_resource);
        if (!texture)
        {
            m_resource = resources.UploadTexture(m_asset);
            texture = resources.TryGetTexture(m_resource);
        }
        if (!texture)
        {
            m_skybox = {};
            m_status = EnvironmentResourceStatus::ResourceUploadFailed;
            return m_status;
        }

        EnvironmentTextureResource resolved{
            .resource = m_resource,
            .texture = texture->texture,
            .defaultView = texture->defaultView,
            .sampler = texture->sampler,
            .desc = BuildTextureDesc(*texture),
        };
        if (!resolved.IsValid())
        {
            m_skybox = {};
            m_status = EnvironmentResourceStatus::InvalidTextureContract;
            return m_status;
        }

        m_skybox = resolved;
        m_status = EnvironmentResourceStatus::Ready;
        return m_status;
    }

    void EnvironmentResourceResolver::Reset()
    {
        m_reference = {};
        m_asset = Asset::TextureHandle::Invalid();
        m_resource = Resource::TextureHandle::Invalid();
        m_skybox = {};
        m_status = EnvironmentResourceStatus::Disabled;
        m_attemptedAssetLoad = false;
        m_wasEnabled = false;
    }

    RGTextureHandle PublishEnvironmentSkybox(RenderGraph& graph, RenderGraphBlackboard& blackboard, const EnvironmentTextureResource& skybox, const ImportedTextureMap& pendingUploads)
    {
        if (!skybox.IsValid())
            return {};

        RGTextureHandle graphTexture;
        if (const auto upload = pendingUploads.find(skybox.texture); upload != pendingUploads.end())
            graphTexture = upload->second;
        else
            graphTexture = graph.ImportTexture("Environment.Skybox", skybox.texture, skybox.desc, ResourceState::ShaderResource, ResourceState::ShaderResource);

        blackboard.SetTexture(std::string(RenderGraphSemantic::EnvironmentSkybox), graphTexture);
        return graphTexture;
    }
} // namespace ChikaEngine::Render
