#pragma once

#include "ChikaEngine/AssetHandle.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/base/SlotMap.h"
#include "RenderResourceRequest.hpp"
#include "ResourceHandle.hpp"
#include "ResourceLayout.hpp"
#include <functional>
#include <mutex>
#include <unordered_map>
namespace ChikaEngine::Resource
{

    class ResourceManager
    {
      public:
        // 依赖注入
        ResourceManager(Render::IRHIDevice& rhi, Asset::AssetManager& assetMgr);
        ~ResourceManager();

        // 带缓冲的上传
        MeshHandle UploadMesh(Asset::MeshHandle assetHandle);
        TextureHandle UploadTexture(Asset::TextureHandle assetHandle);
        MaterialHandle UploadMaterial(Asset::MaterialHandle assetHandle);

        const MeshGPU& GetMesh(MeshHandle handle) const;
        const TextureGPU& GetTexture(TextureHandle handle) const;
        const MaterialGPU& GetMaterial(MaterialHandle handle) const;

      public:
        std::vector<BufferUploadRequest> GetBufferUploadJobs();
        std::vector<TextureUploadRequest> GetTextureUploadJobs();

      private:
        MeshHandle _UploadMesh(Asset::MeshHandle assetHandle);
        TextureHandle _UploadTexture(Asset::TextureHandle assetHandle);
        MaterialHandle _UploadMaterial(Asset::MaterialHandle assetHandle);

        void SubmitImmediate(const std::function<void(Render::IRHICommandList*)>& recordFunc);

      private:
        Render::IRHIDevice& m_rhi;
        Asset::AssetManager& m_assetManager;

        Core::SlotMap<MeshHandle, MeshGPU> m_meshes;
        Core::SlotMap<TextureHandle, TextureGPU> m_textures;
        Core::SlotMap<MaterialHandle, MaterialGPU> m_materials;

        std::unordered_map<Asset::MeshHandle, MeshHandle> m_meshCache;
        std::unordered_map<Asset::TextureHandle, TextureHandle> m_textureCache;
        std::unordered_map<Asset::MaterialHandle, MaterialHandle> m_materialCache;

      private:
        std::mutex m_uploadMutex;
        // 延迟提交
        std::vector<BufferUploadRequest> m_pendingBufferUploads;
        std::vector<TextureUploadRequest> m_pendingTextureUploads;
    };
} // namespace ChikaEngine::Resource