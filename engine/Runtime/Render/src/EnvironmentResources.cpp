#include "ChikaEngine/EnvironmentResources.hpp"

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderGraphBlackboard.hpp"
#include "ChikaEngine/RenderSettings.hpp"
#include "ChikaEngine/ResourceLayout.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include "ChikaEngine/debug/log_macros.h"

#include <chrono>
#include <exception>

namespace ChikaEngine::Render
{
    namespace
    {
        constexpr std::string_view DEFAULT_SKYBOX_GUID = "67d279940ad24613a5be745bec80fdb2";
        constexpr std::string_view DEFAULT_SKYBOX_PATH = "Assets/Textures/Skybox/default-skybox.texture";

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

        Asset::AssetReference DefaultSkyboxReference()
        {
            return Asset::AssetReference({ std::string(DEFAULT_SKYBOX_GUID) }, Asset::AssetType::Texture, {}, std::string(DEFAULT_SKYBOX_PATH));
        }
    } // namespace

    std::string_view EnvironmentResourceStatusName(EnvironmentResourceStatus status)
    {
        switch (status)
        {
        case EnvironmentResourceStatus::Disabled:
            return "disabled";
        case EnvironmentResourceStatus::Loading:
            return "loading";
        case EnvironmentResourceStatus::MissingReference:
            return "missing-reference";
        case EnvironmentResourceStatus::AssetLoadFailed:
            return "asset-load-failed";
        case EnvironmentResourceStatus::TextureDecodeFailed:
            return "texture-decode-failed";
        case EnvironmentResourceStatus::UnsupportedEXR:
            return "unsupported-exr";
        case EnvironmentResourceStatus::InvalidTexturePayload:
            return "invalid-texture-payload";
        case EnvironmentResourceStatus::ProjectionFailed:
            return "projection-failed";
        case EnvironmentResourceStatus::FaceSizeLimitExceeded:
            return "face-size-limit-exceeded";
        case EnvironmentResourceStatus::ResourceUploadFailed:
            return "resource-upload-failed";
        case EnvironmentResourceStatus::InvalidTextureContract:
            return "invalid-texture-contract";
        case EnvironmentResourceStatus::FallbackUnavailable:
            return "fallback-unavailable";
        case EnvironmentResourceStatus::Ready:
            return "ready";
        case EnvironmentResourceStatus::ReadyFallback:
            return "ready-fallback";
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
            m_usingFallback = false;
            return m_status;
        }

        const bool referenceChanged = !HasSameReference(settings.skybox) || m_useFallback != settings.useFallback;
        if (!m_wasEnabled || referenceChanged)
        {
            m_reference = settings.skybox;
            m_activeReference = {};
            m_asset = Asset::TextureHandle::Invalid();
            m_assetLoad = {};
            m_resource = Resource::TextureHandle::Invalid();
            m_skybox = {};
            m_attemptedAssetLoad = false;
            m_useFallback = settings.useFallback;
            m_usingFallback = false;
        }
        m_wasEnabled = true;

        if (m_usingFallback)
        {
            const EnvironmentResourceStatus fallbackStatus = ResolveReference(DefaultSkyboxReference(), assets, resources);
            if (fallbackStatus == EnvironmentResourceStatus::Loading)
            {
                m_status = EnvironmentResourceStatus::Loading;
                return m_status;
            }
            m_status = fallbackStatus == EnvironmentResourceStatus::Ready ? EnvironmentResourceStatus::ReadyFallback : EnvironmentResourceStatus::FallbackUnavailable;
            return m_status;
        }

        const EnvironmentResourceStatus primaryStatus = IsReferenceConfigured(m_reference) ? ResolveReference(m_reference, assets, resources) : EnvironmentResourceStatus::MissingReference;
        if (primaryStatus == EnvironmentResourceStatus::Loading)
        {
            m_status = primaryStatus;
            return m_status;
        }
        if (primaryStatus == EnvironmentResourceStatus::Ready || !settings.useFallback)
        {
            m_status = primaryStatus;
            return m_status;
        }

        LOG_WARN("EnvironmentResource", "Primary environment '{}' failed with status={}; attempting packaged LDR fallback", m_reference.diagnosticPath, EnvironmentResourceStatusName(primaryStatus));

        m_activeReference = {};
        m_asset = Asset::TextureHandle::Invalid();
        m_assetLoad = {};
        m_resource = Resource::TextureHandle::Invalid();
        m_skybox = {};
        m_attemptedAssetLoad = false;
        m_usingFallback = true;
        const EnvironmentResourceStatus fallbackStatus = ResolveReference(DefaultSkyboxReference(), assets, resources);
        if (fallbackStatus == EnvironmentResourceStatus::Loading)
        {
            m_status = EnvironmentResourceStatus::Loading;
            return m_status;
        }
        m_status = fallbackStatus == EnvironmentResourceStatus::Ready ? EnvironmentResourceStatus::ReadyFallback : EnvironmentResourceStatus::FallbackUnavailable;
        if (m_status == EnvironmentResourceStatus::ReadyFallback)
            LOG_INFO("EnvironmentResource", "Packaged LDR environment fallback is ready: {}", DEFAULT_SKYBOX_PATH);
        else
            LOG_WARN("EnvironmentResource", "Packaged LDR environment fallback failed with status={}; renderer will use fallbackColor", EnvironmentResourceStatusName(fallbackStatus));
        return m_status;
    }

    EnvironmentResourceStatus EnvironmentResourceResolver::ResolveReference(const Asset::AssetReference& reference, Asset::AssetManager& assets, Resource::ResourceManager& resources)
    {
        if (!IsReferenceConfigured(reference))
            return EnvironmentResourceStatus::MissingReference;

        if (m_activeReference.guid != reference.guid || m_activeReference.diagnosticPath != reference.diagnosticPath)
        {
            m_activeReference = reference;
            m_asset = Asset::TextureHandle::Invalid();
            m_assetLoad = {};
            m_resource = Resource::TextureHandle::Invalid();
            m_skybox = {};
            m_attemptedAssetLoad = false;
        }

        if (!m_attemptedAssetLoad)
        {
            const Asset::AssetRecord* record = assets.ResolveReference(reference, Asset::AssetType::Texture, "Environment.Skybox");
            if (!record)
                return EnvironmentResourceStatus::AssetLoadFailed;

            try
            {
                m_assetLoad = assets.LoadTextureAsync(record->sourcePath.string());
            }
            catch (const std::exception& exception)
            {
                LOG_ERROR("EnvironmentResource", "Could not schedule environment texture '{}': {}", record->sourcePath.string(), exception.what());
                return EnvironmentResourceStatus::AssetLoadFailed;
            }
            m_attemptedAssetLoad = true;
        }

        if (!m_asset.IsValid() && m_assetLoad.valid())
        {
            if (m_assetLoad.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                return EnvironmentResourceStatus::Loading;

            try
            {
                m_asset = m_assetLoad.get();
            }
            catch (const std::exception& exception)
            {
                LOG_ERROR("EnvironmentResource", "Asynchronous environment texture load failed for '{}': {}", reference.diagnosticPath, exception.what());
            }
            m_assetLoad = {};
        }
        if (!m_asset.IsValid())
        {
            switch (assets.GetTextureLoadStatus(reference))
            {
            case Asset::TextureLoadStatus::DecodeFailed:
                return EnvironmentResourceStatus::TextureDecodeFailed;
            case Asset::TextureLoadStatus::UnsupportedEXR:
                return EnvironmentResourceStatus::UnsupportedEXR;
            case Asset::TextureLoadStatus::InvalidFloatPayload:
            case Asset::TextureLoadStatus::InvalidPayloadLayout:
                return EnvironmentResourceStatus::InvalidTexturePayload;
            case Asset::TextureLoadStatus::InvalidProjection:
                return EnvironmentResourceStatus::ProjectionFailed;
            case Asset::TextureLoadStatus::FaceSizeLimitExceeded:
                return EnvironmentResourceStatus::FaceSizeLimitExceeded;
            default:
                return EnvironmentResourceStatus::AssetLoadFailed;
            }
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
            const Resource::TextureUploadStatus uploadStatus = resources.GetTextureUploadStatus(m_asset);
            if (uploadStatus == Resource::TextureUploadStatus::InvalidPayload)
                return EnvironmentResourceStatus::InvalidTexturePayload;
            if (uploadStatus == Resource::TextureUploadStatus::DimensionLimitExceeded)
                return EnvironmentResourceStatus::FaceSizeLimitExceeded;
            return EnvironmentResourceStatus::ResourceUploadFailed;
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
            return EnvironmentResourceStatus::InvalidTextureContract;
        }

        m_skybox = resolved;
        return EnvironmentResourceStatus::Ready;
    }

    void EnvironmentResourceResolver::Reset()
    {
        m_reference = {};
        m_activeReference = {};
        m_asset = Asset::TextureHandle::Invalid();
        m_assetLoad = {};
        m_resource = Resource::TextureHandle::Invalid();
        m_skybox = {};
        m_status = EnvironmentResourceStatus::Disabled;
        m_attemptedAssetLoad = false;
        m_wasEnabled = false;
        m_useFallback = true;
        m_usingFallback = false;
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
