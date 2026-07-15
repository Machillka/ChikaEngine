#pragma once

#include "ChikaEngine/AssetHandle.hpp"
#include "ChikaEngine/AssetReference.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderGraphHandle.hpp"
#include "ChikaEngine/ResourceHandle.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace ChikaEngine::Asset
{
    class AssetManager;
}

namespace ChikaEngine::Resource
{
    class ResourceManager;
}

namespace ChikaEngine::Render
{
    class RenderGraph;
    class RenderGraphBlackboard;
    struct EnvironmentSettings;

    /** @brief 表示环境 Cubemap 从配置到 GPU 资源桥接后的状态。 */
    enum class EnvironmentResourceStatus : uint8_t
    {
        Disabled,
        MissingReference,
        AssetLoadFailed,
        ResourceUploadFailed,
        InvalidTextureContract,
        Ready,
    };

    std::string_view EnvironmentResourceStatusName(EnvironmentResourceStatus status);

    /** @brief 保存可安全导入 RenderGraph 的后端无关 Cubemap 资源。 */
    struct EnvironmentTextureResource
    {
        Resource::TextureHandle resource;
        TextureHandle texture;
        TextureViewHandle defaultView;
        SamplerHandle sampler;
        TextureDesc desc;

        bool IsValid() const;
    };

    using ImportedTextureMap = std::unordered_map<TextureHandle, RGTextureHandle>;

    /**
     * @brief 解析并缓存 Skybox 资源，同时处理 ResourceManager hot-reload 后的 stale handle。
     *
     * Resolver 不拥有 Asset、Resource 或 RHI 对象；所有权继续由对应 Manager 维护。
     */
    class EnvironmentResourceResolver
    {
      public:
        EnvironmentResourceStatus Update(const EnvironmentSettings& settings, Asset::AssetManager& assets, Resource::ResourceManager& resources);
        void Reset();

        EnvironmentResourceStatus GetStatus() const
        {
            return m_status;
        }

        const EnvironmentTextureResource& GetSkybox() const
        {
            return m_skybox;
        }

      private:
        bool HasSameReference(const Asset::AssetReference& reference) const;

        Asset::AssetReference m_reference;
        Asset::TextureHandle m_asset;
        Resource::TextureHandle m_resource;
        EnvironmentTextureResource m_skybox;
        EnvironmentResourceStatus m_status = EnvironmentResourceStatus::Disabled;
        bool m_attemptedAssetLoad = false;
        bool m_wasEnabled = false;
    };

    /**
     * @brief 发布 Skybox Blackboard 语义；同帧有 upload 时复用其 RG handle。
     */
    RGTextureHandle PublishEnvironmentSkybox(RenderGraph& graph, RenderGraphBlackboard& blackboard, const EnvironmentTextureResource& skybox, const ImportedTextureMap& pendingUploads);
} // namespace ChikaEngine::Render
