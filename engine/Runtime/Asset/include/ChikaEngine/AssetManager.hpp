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
#include "AssetReference.hpp"
#include "ChikaEngine/base/SlotMap.h"
#include <condition_variable>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
namespace ChikaEngine::Jobs
{
    class JobSystem;
}

namespace ChikaEngine::Asset
{
    struct AssetReloadEvent
    {
        AssetGuid guid;
        AssetType type = AssetType::Unknown;
        std::filesystem::path sourcePath;
    };

    /** @brief 描述 AssetManager 在当前 RuntimeMode 下允许的资产能力。 */
    struct AssetManagerCreateInfo
    {
        std::filesystem::path assetRoot = "Assets";
        bool createRoot = true;
        bool scanAssets = true;
        bool createMissingMeta = true;
        bool importAssets = true;
        bool enableHotReload = true;
        Jobs::JobSystem* jobSystem = nullptr;
    };

    class AssetManager
    {
      public:
        AssetManager() = default;
        ~AssetManager();

        bool Initialize(const std::filesystem::path& assetRoot = "Assets", bool importAssets = true);
        bool Initialize(const AssetManagerCreateInfo& createInfo);
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

        /** @brief 按稳定 AssetReference 加载 Texture，并校验引用类型。 */
        TextureHandle LoadTexture(const AssetReference& reference);
        /** @brief 按稳定 AssetReference 加载 Mesh，并校验引用类型。 */
        MeshHandle LoadMesh(const AssetReference& reference);
        /** @brief 按稳定 AssetReference 加载 Shader，并校验引用类型。 */
        ShaderHandle LoadShader(const AssetReference& reference);
        /** @brief 按稳定 AssetReference 加载 Material，并校验引用类型。 */
        MaterialHandle LoadMaterial(const AssetReference& reference);
        /** @brief 按稳定 AssetReference 加载 Shader Template，并校验引用类型。 */
        ShaderTemplateHandle LoadShaderTemplate(const AssetReference& reference);
        /** @brief 按源资产 GUID 和 SubAsset 契约加载 Animation Clip。 */
        AnimationClipHandle LoadAnimationClip(const AssetReference& reference);

        /**
         * @brief 将稳定引用解析到当前 AssetDatabase 记录。
         *
         * 正式引用按 GUID 查询；仅开发态兼容引用可使用 diagnosticPath 回退。
         */
        const AssetRecord* ResolveReference(const AssetReference& reference, AssetType requiredType, std::string_view context = {}) const;

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
        template <typename Result, typename Function> std::shared_future<Result> LaunchAsync(std::string_view jobName, std::string cacheKey, std::unordered_map<std::string, std::shared_future<Result>>& requests, Function&& function)
        {
            auto promise = std::make_shared<std::promise<Result>>();
            std::shared_future<Result> future = promise->get_future().share();
            {
                std::lock_guard lock(m_asyncMutex);
                if (m_shuttingDown)
                    throw std::runtime_error("AssetManager is shutting down");
                const auto existing = requests.find(cacheKey);
                if (existing != requests.end())
                    return existing->second;
                ++m_activeAsyncLoads;
                requests.emplace(cacheKey, future);
            }

            auto task = [this, promise, requestKey = cacheKey, &requests, function = std::forward<Function>(function)](bool propagateFailure) mutable
            {
                try
                {
                    promise->set_value(function());
                    FinishAsyncLoad(requests, requestKey, false);
                }
                catch (...)
                {
                    const std::exception_ptr exception = std::current_exception();
                    FinishAsyncLoad(requests, requestKey, true);
                    promise->set_exception(exception);
                    if (propagateFailure)
                        std::rethrow_exception(exception);
                }
            };

            if (!m_jobSystem)
            {
                task(false);
                return future;
            }

            if (!ScheduleAssetJob(jobName, [task = std::move(task)]() mutable { task(true); }))
            {
                FinishAsyncLoad(requests, cacheKey, true);
                promise->set_exception(std::make_exception_ptr(std::runtime_error("JobSystem rejected asset load")));
                return future;
            }
            return future;
        }

        /** @brief Completes async accounting and removes failed requests so callers may retry them. */
        template <typename Result> void FinishAsyncLoad(std::unordered_map<std::string, std::shared_future<Result>>& requests, const std::string& cacheKey, bool eraseRequest)
        {
            std::lock_guard lock(m_asyncMutex);
            if (eraseRequest)
                requests.erase(cacheKey);
            --m_activeAsyncLoads;
            m_asyncCondition.notify_all();
        }

        /** @brief Invalidates one successful request future when its underlying handle is unloaded. */
        template <typename Result> void ClearAsyncRequest(std::unordered_map<std::string, std::shared_future<Result>>& requests, const std::string& cacheKey)
        {
            std::lock_guard lock(m_asyncMutex);
            requests.erase(cacheKey);
        }

        template <typename Handle, typename Data, typename Loader> Handle LoadCachedAsset(const std::string& path, Core::SlotMap<Handle, Data>& slots, std::unordered_map<std::string, Handle>& cache, Loader&& loader)
        {
            std::string cachePath;
            std::string loadPath;
            {
                std::lock_guard lock(m_assetMutex);
                cachePath = NormalizeCachePath(path);
                const auto cached = cache.find(cachePath);
                if (cached != cache.end())
                    return cached->second;
                loadPath = ResolveImportedPath(path);
            }

            auto data = loader(loadPath);
            if (!data)
                return Handle::Invalid();

            std::lock_guard lock(m_assetMutex);
            const auto cached = cache.find(cachePath);
            if (cached != cache.end())
                return cached->second;
            const Handle handle = slots.Create(*data);
            cache[cachePath] = handle;
            TrackLoadedFile(cachePath, loadPath);
            return handle;
        }

        bool ScheduleAssetJob(std::string_view name, std::function<void()> function);
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

        template <typename Handle, typename Data> bool UnloadFromCache(Handle handle, Core::SlotMap<Handle, Data>& slots, std::unordered_map<std::string, Handle>& cache, std::string* unloadedPath = nullptr)
        {
            std::lock_guard lock(m_assetMutex);
            if (!slots.Get(handle))
                return false;
            for (auto it = cache.begin(); it != cache.end();)
            {
                if (it->second == handle)
                {
                    if (unloadedPath && unloadedPath->empty())
                        *unloadedPath = it->first;
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
        bool m_enableImporting = true;
        bool m_enableHotReload = true;
        std::chrono::steady_clock::time_point m_nextHotReloadPoll{};
        std::unordered_map<std::string, std::filesystem::file_time_type> m_loadedWriteTimes;
        std::unordered_map<size_t, std::function<void(const AssetReloadEvent&)>> m_reloadCallbacks;
        size_t m_nextReloadSubscriptionId = 1;

        std::mutex m_asyncMutex;
        std::condition_variable m_asyncCondition;
        size_t m_activeAsyncLoads = 0;
        bool m_shuttingDown = false;
        Jobs::JobSystem* m_jobSystem = nullptr;
        std::unordered_map<std::string, std::shared_future<TextureHandle>> m_textureAsyncRequests;
        std::unordered_map<std::string, std::shared_future<MeshHandle>> m_meshAsyncRequests;
        std::unordered_map<std::string, std::shared_future<ShaderHandle>> m_shaderAsyncRequests;
        std::unordered_map<std::string, std::shared_future<MaterialHandle>> m_materialAsyncRequests;

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
