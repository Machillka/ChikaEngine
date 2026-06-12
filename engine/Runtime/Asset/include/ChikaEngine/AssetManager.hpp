/*!
 * @file AssetManager.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 资产的概念, 底层读取数据 所以起成 Asset 空间
 *
 * 数据流动方向： 硬盘 -> CPU
 * @version 0.1
 * @date 2026-03-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "AssetDatabase.hpp"
#include "AssetImporter.hpp"
#include "AssetAnimation.hpp"
#include "AssetHandle.hpp"
#include "AssetLayouts.hpp"
#include "ChikaEngine/base/SlotMap.h"
#include <condition_variable>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
namespace ChikaEngine::Asset
{
    struct AssetReloadEvent
    {
        AssetGuid guid;
        AssetType type = AssetType::Unknown;
        std::filesystem::path sourcePath;
    };

    class AssetManager
    {
      public:
        AssetManager() = default;
        ~AssetManager();

        bool Initialize(const std::filesystem::path& assetRoot = "Assets", bool importAssets = true);
        void Shutdown();

        AssetDatabase& GetDatabase()
        {
            return m_database;
        }
        ImporterRegistry& GetImporters()
        {
            return m_importers;
        }
        size_t ImportAll();
        size_t TickHotReload();
        size_t SubscribeReload(std::function<void(const AssetReloadEvent&)> callback);
        void UnsubscribeReload(size_t subscriptionId);

        // load from disk
        TextureHandle LoadTexture(const std::string& path);
        MeshHandle LoadMesh(const std::string& path);
        ShaderHandle LoadShader(const std::string& path);
        MaterialHandle LoadMaterial(const std::string& path);
        ShaderTemplateHandle LoadShaderTemplate(const std::string& path);

        AnimationClipHandle LoadAnimationClip(const std::string& path);

        TextureHandle LoadTexture(const AssetGuid& guid);
        MeshHandle LoadMesh(const AssetGuid& guid);
        ShaderHandle LoadShader(const AssetGuid& guid);
        MaterialHandle LoadMaterial(const AssetGuid& guid);
        ShaderTemplateHandle LoadShaderTemplate(const AssetGuid& guid);
        AnimationClipHandle LoadAnimationClip(const AssetGuid& guid);

        std::shared_future<TextureHandle> LoadTextureAsync(std::string path);
        std::shared_future<MeshHandle> LoadMeshAsync(std::string path);
        std::shared_future<ShaderHandle> LoadShaderAsync(std::string path);
        std::shared_future<MaterialHandle> LoadMaterialAsync(std::string path);

        bool Unload(TextureHandle handle);
        bool Unload(MeshHandle handle);
        bool Unload(ShaderHandle handle);
        bool Unload(MaterialHandle handle);
        bool Unload(ShaderTemplateHandle handle);
        bool Unload(AnimationClipHandle handle);
        void UnloadAll();

      public:
        // 查询
        const TextureData* GetTexture(TextureHandle h) const;
        const MeshData* GetMesh(MeshHandle h) const;
        const ShaderData* GetShader(ShaderHandle h) const;
        const MaterialData* GetMaterial(MaterialHandle h) const;
        const ShaderTemplateData* GetShaderTemplate(ShaderTemplateHandle h) const;

        const AnimationClipData* GetAnimationClip(AnimationClipHandle h) const;

      private:
        template <typename Result, typename Function> std::shared_future<Result> LaunchAsync(Function&& function)
        {
            {
                std::lock_guard lock(m_asyncMutex);
                if (m_shuttingDown)
                    throw std::runtime_error("AssetManager is shutting down");
                ++m_activeAsyncLoads;
            }

            try
            {
                return std::async(std::launch::async,
                                  [this, function = std::forward<Function>(function)]() mutable -> Result
                                  {
                                      try
                                      {
                                          Result result = function();
                                          FinishAsyncLoad();
                                          return result;
                                      }
                                      catch (...)
                                      {
                                          FinishAsyncLoad();
                                          throw;
                                      }
                                  })
                    .share();
            }
            catch (...)
            {
                FinishAsyncLoad();
                throw;
            }
        }

        void FinishAsyncLoad();
        std::string NormalizeCachePath(const std::string& path) const;
        std::string ResolveImportedPath(const std::string& path);
        void TrackLoadedFile(const std::string& cachePath, const std::string& loadPath);
        void PublishReload(const std::string& cachePath);

        template <typename Handle, typename Data, typename Loader> size_t ReloadChanged(std::unordered_map<std::string, Handle>& cache, Core::SlotMap<Handle, Data>& slots, Loader&& loader)
        {
            size_t reloadCount = 0;
            for (const auto& [cachePath, handle] : cache)
            {
                const std::string loadPath = ResolveImportedPath(cachePath);
                std::error_code error;
                const auto writeTime = std::filesystem::last_write_time(loadPath, error);
                if (error)
                    continue;

                const auto tracked = m_loadedWriteTimes.find(cachePath);
                if (tracked == m_loadedWriteTimes.end())
                {
                    m_loadedWriteTimes[cachePath] = writeTime;
                    continue;
                }
                if (tracked->second == writeTime)
                    continue;

                auto data = loader(loadPath);
                Data* current = slots.Get(handle);
                if (!data || !current)
                    continue;

                *current = std::move(*data);
                tracked->second = writeTime;
                ++reloadCount;
                PublishReload(cachePath);
            }
            return reloadCount;
        }

        template <typename Handle, typename Data> bool UnloadFromCache(Handle handle, Core::SlotMap<Handle, Data>& slots, std::unordered_map<std::string, Handle>& cache)
        {
            std::lock_guard lock(m_assetMutex);
            if (!slots.Get(handle))
                return false;
            for (auto it = cache.begin(); it != cache.end();)
            {
                if (it->second == handle)
                {
                    m_loadedWriteTimes.erase(it->first);
                    it = cache.erase(it);
                }
                else
                    ++it;
            }
            slots.Destroy(handle);
            return true;
        }

      private:
        mutable std::recursive_mutex m_assetMutex;
        AssetDatabase m_database;
        ImporterRegistry m_importers;
        bool m_initialized = false;
        std::chrono::steady_clock::time_point m_nextHotReloadPoll{};
        std::unordered_map<std::string, std::filesystem::file_time_type> m_loadedWriteTimes;
        std::unordered_map<size_t, std::function<void(const AssetReloadEvent&)>> m_reloadCallbacks;
        size_t m_nextReloadSubscriptionId = 1;

        std::mutex m_asyncMutex;
        std::condition_variable m_asyncCondition;
        size_t m_activeAsyncLoads = 0;
        bool m_shuttingDown = false;

        Core::SlotMap<TextureHandle, TextureData> m_textures;
        Core::SlotMap<MeshHandle, MeshData> m_meshes;
        Core::SlotMap<ShaderHandle, ShaderData> m_shaders;
        Core::SlotMap<MaterialHandle, MaterialData> m_materials;
        Core::SlotMap<ShaderTemplateHandle, ShaderTemplateData> m_shaderTemplates;

        Core::SlotMap<AnimationClipHandle, AnimationClipData> m_animationClips;

        // 路径缓存（避免重复加载）
        std::unordered_map<std::string, TextureHandle> m_texturePathCache;
        std::unordered_map<std::string, MeshHandle> m_meshPathCache;
        std::unordered_map<std::string, ShaderHandle> m_shaderPathCache;
        std::unordered_map<std::string, MaterialHandle> m_materialPathCache;
        std::unordered_map<std::string, ShaderTemplateHandle> m_shaderTemplatePathCache;

        std::unordered_map<std::string, AnimationClipHandle> m_animationClipPathCache;
    };
} // namespace ChikaEngine::Asset
