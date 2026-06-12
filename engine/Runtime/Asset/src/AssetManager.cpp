#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/AnimationLoader.hpp"
#include "ChikaEngine/MaterialLoader.hpp"
#include "ChikaEngine/MeshLoader.hpp"
#include "ChikaEngine/ShaderLoader.hpp"
#include "ChikaEngine/ShaderTemplateLoader.hpp"
#include "ChikaEngine/TextureLoader.hpp"

namespace ChikaEngine::Asset
{
    namespace
    {
        template <typename Handle, typename Data, typename Loader> Handle LoadCached(const std::string& cachePath, const std::string& loadPath, Core::SlotMap<Handle, Data>& slots, std::unordered_map<std::string, Handle>& cache, Loader&& loader)
        {
            const auto cached = cache.find(cachePath);
            if (cached != cache.end())
                return cached->second;

            auto data = loader(loadPath);
            if (!data)
                return Handle::Invalid();

            const Handle handle = slots.Create(*data);
            cache[cachePath] = handle;
            return handle;
        }
    } // namespace

    AssetManager::~AssetManager()
    {
        Shutdown();
    }

    bool AssetManager::Initialize(const std::filesystem::path& assetRoot, bool importAssets)
    {
        Shutdown();
        {
            std::lock_guard asyncLock(m_asyncMutex);
            m_shuttingDown = false;
        }
        m_importers = ImporterRegistry::CreateDefault();
        m_initialized = m_database.Initialize(assetRoot);
        if (m_initialized && importAssets)
            ImportAll();
        return m_initialized;
    }

    void AssetManager::Shutdown()
    {
        {
            std::unique_lock lock(m_asyncMutex);
            m_shuttingDown = true;
            m_asyncCondition.wait(lock, [this] { return m_activeAsyncLoads == 0; });
        }
        UnloadAll();
        m_database.Shutdown();
        {
            std::lock_guard lock(m_assetMutex);
            m_reloadCallbacks.clear();
            m_nextReloadSubscriptionId = 1;
        }
        m_initialized = false;
    }

    size_t AssetManager::ImportAll()
    {
        std::lock_guard lock(m_assetMutex);
        return m_importers.ImportAll(m_database);
    }

    size_t AssetManager::TickHotReload()
    {
        std::lock_guard lock(m_assetMutex);
        const auto now = std::chrono::steady_clock::now();
        if (!m_initialized || now < m_nextHotReloadPoll)
            return 0;
        m_nextHotReloadPoll = now + std::chrono::milliseconds(500);

        for (const AssetGuid& guid : m_database.PollChangedAssets())
            m_importers.Import(m_database, guid);

        size_t count = 0;
        count += ReloadChanged(m_meshPathCache, m_meshes, MeshLoader::Load);
        count += ReloadChanged(m_texturePathCache, m_textures, TextureLoader::Load);
        count += ReloadChanged(m_shaderPathCache, m_shaders, ShaderLoader::Load);
        count += ReloadChanged(m_materialPathCache, m_materials, MaterialLoader::Load);
        count += ReloadChanged(m_shaderTemplatePathCache, m_shaderTemplates, ShaderTemplateLoader::Load);
        count += ReloadChanged(m_animationClipPathCache, m_animationClips, AnimationLoader::Load);
        return count;
    }

    size_t AssetManager::SubscribeReload(std::function<void(const AssetReloadEvent&)> callback)
    {
        std::lock_guard lock(m_assetMutex);
        const size_t id = m_nextReloadSubscriptionId++;
        m_reloadCallbacks[id] = std::move(callback);
        return id;
    }

    void AssetManager::UnsubscribeReload(size_t subscriptionId)
    {
        std::lock_guard lock(m_assetMutex);
        m_reloadCallbacks.erase(subscriptionId);
    }

    MeshHandle AssetManager::LoadMesh(const std::string& path)
    {
        std::lock_guard lock(m_assetMutex);
        const std::string cachePath = NormalizeCachePath(path);
        const std::string loadPath = ResolveImportedPath(path);
        const auto handle = LoadCached(cachePath, loadPath, m_meshes, m_meshPathCache, MeshLoader::Load);
        TrackLoadedFile(cachePath, loadPath);
        return handle;
    }

    TextureHandle AssetManager::LoadTexture(const std::string& path)
    {
        std::lock_guard lock(m_assetMutex);
        const std::string cachePath = NormalizeCachePath(path);
        const std::string loadPath = ResolveImportedPath(path);
        const auto handle = LoadCached(cachePath, loadPath, m_textures, m_texturePathCache, TextureLoader::Load);
        TrackLoadedFile(cachePath, loadPath);
        return handle;
    }

    ShaderHandle AssetManager::LoadShader(const std::string& path)
    {
        std::lock_guard lock(m_assetMutex);
        const std::string cachePath = NormalizeCachePath(path);
        const std::string loadPath = ResolveImportedPath(path);
        const auto handle = LoadCached(cachePath, loadPath, m_shaders, m_shaderPathCache, ShaderLoader::Load);
        TrackLoadedFile(cachePath, loadPath);
        return handle;
    }

    ShaderTemplateHandle AssetManager::LoadShaderTemplate(const std::string& path)
    {
        std::lock_guard lock(m_assetMutex);
        const std::string cachePath = NormalizeCachePath(path);
        const std::string loadPath = ResolveImportedPath(path);
        const auto handle = LoadCached(cachePath, loadPath, m_shaderTemplates, m_shaderTemplatePathCache, ShaderTemplateLoader::Load);
        TrackLoadedFile(cachePath, loadPath);
        return handle;
    }

    MaterialHandle AssetManager::LoadMaterial(const std::string& path)
    {
        std::lock_guard lock(m_assetMutex);
        const std::string cachePath = NormalizeCachePath(path);
        const std::string loadPath = ResolveImportedPath(path);
        const auto handle = LoadCached(cachePath, loadPath, m_materials, m_materialPathCache, MaterialLoader::Load);
        TrackLoadedFile(cachePath, loadPath);
        return handle;
    }

    AnimationClipHandle AssetManager::LoadAnimationClip(const std::string& path)
    {
        std::lock_guard lock(m_assetMutex);
        const std::string cachePath = NormalizeCachePath(path);
        const std::string loadPath = ResolveImportedPath(path);
        const auto handle = LoadCached(cachePath, loadPath, m_animationClips, m_animationClipPathCache, AnimationLoader::Load);
        TrackLoadedFile(cachePath, loadPath);
        return handle;
    }

    TextureHandle AssetManager::LoadTexture(const AssetGuid& guid)
    {
        std::lock_guard lock(m_assetMutex);
        const AssetRecord* record = m_database.FindByGuid(guid);
        return record ? LoadTexture(record->sourcePath.string()) : TextureHandle::Invalid();
    }

    MeshHandle AssetManager::LoadMesh(const AssetGuid& guid)
    {
        std::lock_guard lock(m_assetMutex);
        const AssetRecord* record = m_database.FindByGuid(guid);
        return record ? LoadMesh(record->sourcePath.string()) : MeshHandle::Invalid();
    }

    ShaderHandle AssetManager::LoadShader(const AssetGuid& guid)
    {
        std::lock_guard lock(m_assetMutex);
        const AssetRecord* record = m_database.FindByGuid(guid);
        return record ? LoadShader(record->sourcePath.string()) : ShaderHandle::Invalid();
    }

    MaterialHandle AssetManager::LoadMaterial(const AssetGuid& guid)
    {
        std::lock_guard lock(m_assetMutex);
        const AssetRecord* record = m_database.FindByGuid(guid);
        return record ? LoadMaterial(record->sourcePath.string()) : MaterialHandle::Invalid();
    }

    ShaderTemplateHandle AssetManager::LoadShaderTemplate(const AssetGuid& guid)
    {
        std::lock_guard lock(m_assetMutex);
        const AssetRecord* record = m_database.FindByGuid(guid);
        return record ? LoadShaderTemplate(record->sourcePath.string()) : ShaderTemplateHandle::Invalid();
    }

    AnimationClipHandle AssetManager::LoadAnimationClip(const AssetGuid& guid)
    {
        std::lock_guard lock(m_assetMutex);
        const AssetRecord* record = m_database.FindByGuid(guid);
        return record ? LoadAnimationClip(record->sourcePath.string()) : AnimationClipHandle::Invalid();
    }

    std::shared_future<TextureHandle> AssetManager::LoadTextureAsync(std::string path)
    {
        return LaunchAsync<TextureHandle>([this, path = std::move(path)] { return LoadTexture(path); });
    }

    std::shared_future<MeshHandle> AssetManager::LoadMeshAsync(std::string path)
    {
        return LaunchAsync<MeshHandle>([this, path = std::move(path)] { return LoadMesh(path); });
    }

    std::shared_future<ShaderHandle> AssetManager::LoadShaderAsync(std::string path)
    {
        return LaunchAsync<ShaderHandle>([this, path = std::move(path)] { return LoadShader(path); });
    }

    std::shared_future<MaterialHandle> AssetManager::LoadMaterialAsync(std::string path)
    {
        return LaunchAsync<MaterialHandle>([this, path = std::move(path)] { return LoadMaterial(path); });
    }

    bool AssetManager::Unload(TextureHandle handle)
    {
        return UnloadFromCache(handle, m_textures, m_texturePathCache);
    }

    bool AssetManager::Unload(MeshHandle handle)
    {
        return UnloadFromCache(handle, m_meshes, m_meshPathCache);
    }

    bool AssetManager::Unload(ShaderHandle handle)
    {
        return UnloadFromCache(handle, m_shaders, m_shaderPathCache);
    }

    bool AssetManager::Unload(MaterialHandle handle)
    {
        return UnloadFromCache(handle, m_materials, m_materialPathCache);
    }

    bool AssetManager::Unload(ShaderTemplateHandle handle)
    {
        return UnloadFromCache(handle, m_shaderTemplates, m_shaderTemplatePathCache);
    }

    bool AssetManager::Unload(AnimationClipHandle handle)
    {
        return UnloadFromCache(handle, m_animationClips, m_animationClipPathCache);
    }

    void AssetManager::UnloadAll()
    {
        std::lock_guard lock(m_assetMutex);
        m_texturePathCache.clear();
        m_meshPathCache.clear();
        m_shaderPathCache.clear();
        m_materialPathCache.clear();
        m_shaderTemplatePathCache.clear();
        m_animationClipPathCache.clear();
        m_loadedWriteTimes.clear();
        m_textures.Clear();
        m_meshes.Clear();
        m_shaders.Clear();
        m_materials.Clear();
        m_shaderTemplates.Clear();
        m_animationClips.Clear();
    }

    const MeshData* AssetManager::GetMesh(MeshHandle handle) const
    {
        std::lock_guard lock(m_assetMutex);
        return m_meshes.Get(handle);
    }

    const TextureData* AssetManager::GetTexture(TextureHandle handle) const
    {
        std::lock_guard lock(m_assetMutex);
        return m_textures.Get(handle);
    }

    const ShaderData* AssetManager::GetShader(ShaderHandle handle) const
    {
        std::lock_guard lock(m_assetMutex);
        return m_shaders.Get(handle);
    }

    const ShaderTemplateData* AssetManager::GetShaderTemplate(ShaderTemplateHandle handle) const
    {
        std::lock_guard lock(m_assetMutex);
        return m_shaderTemplates.Get(handle);
    }

    const MaterialData* AssetManager::GetMaterial(MaterialHandle handle) const
    {
        std::lock_guard lock(m_assetMutex);
        return m_materials.Get(handle);
    }

    const AnimationClipData* AssetManager::GetAnimationClip(AnimationClipHandle handle) const
    {
        std::lock_guard lock(m_assetMutex);
        return m_animationClips.Get(handle);
    }

    void AssetManager::FinishAsyncLoad()
    {
        std::lock_guard lock(m_asyncMutex);
        --m_activeAsyncLoads;
        m_asyncCondition.notify_all();
    }

    std::string AssetManager::NormalizeCachePath(const std::string& path) const
    {
        return AssetDatabase::NormalizePath(path).generic_string();
    }

    std::string AssetManager::ResolveImportedPath(const std::string& path)
    {
        if (!m_initialized)
            return path;

        const AssetRecord* record = m_database.FindByPath(path);
        if (!record)
            return path;

        const ImportResult result = m_importers.Import(m_database, record->guid);
        return result.success ? result.importedPath.string() : path;
    }

    void AssetManager::TrackLoadedFile(const std::string& cachePath, const std::string& loadPath)
    {
        std::error_code error;
        const auto writeTime = std::filesystem::last_write_time(loadPath, error);
        if (!error)
            m_loadedWriteTimes.try_emplace(cachePath, writeTime);
    }

    void AssetManager::PublishReload(const std::string& cachePath)
    {
        AssetReloadEvent event{ .sourcePath = cachePath };
        if (const AssetRecord* record = m_database.FindByPath(cachePath))
        {
            event.guid = record->guid;
            event.type = record->type;
            event.sourcePath = record->sourcePath;
        }

        const auto callbacks = m_reloadCallbacks;
        for (const auto& [id, callback] : callbacks)
        {
            if (callback)
                callback(event);
        }
    }
} // namespace ChikaEngine::Asset
