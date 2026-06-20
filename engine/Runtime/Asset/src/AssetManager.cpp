#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/AnimationLoader.hpp"
#include "ChikaEngine/MaterialLoader.hpp"
#include "ChikaEngine/MeshLoader.hpp"
#include "ChikaEngine/ShaderLoader.hpp"
#include "ChikaEngine/ShaderTemplateLoader.hpp"
#include "ChikaEngine/TextureLoader.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/jobs/JobSystem.hpp"

namespace ChikaEngine::Asset
{
    AssetManager::~AssetManager()
    {
        Shutdown();
    }

    bool AssetManager::Initialize(const std::filesystem::path& assetRoot, bool importAssets)
    {
        return Initialize({
            .assetRoot = assetRoot,
            .importAssets = importAssets,
        });
    }

    bool AssetManager::Initialize(const AssetManagerCreateInfo& createInfo)
    {
        Shutdown();
        {
            std::lock_guard asyncLock(m_asyncMutex);
            m_shuttingDown = false;
            m_jobSystem = createInfo.jobSystem;
        }
        m_importers = ImporterRegistry::CreateDefault();
        m_enableImporting = createInfo.importAssets;
        m_enableHotReload = createInfo.enableHotReload;
        m_initialized = m_database.Initialize({
            .assetRoot = createInfo.assetRoot,
            .createRoot = createInfo.createRoot,
            .scanAssets = createInfo.scanAssets,
            .createMissingMeta = createInfo.createMissingMeta,
        });
        if (m_initialized && createInfo.importAssets)
            ImportAll();
        return m_initialized;
    }

    void AssetManager::Shutdown()
    {
        {
            std::unique_lock lock(m_asyncMutex);
            m_shuttingDown = true;
            m_asyncCondition.wait(lock, [this] { return m_activeAsyncLoads == 0; });
            m_textureAsyncRequests.clear();
            m_meshAsyncRequests.clear();
            m_shaderAsyncRequests.clear();
            m_materialAsyncRequests.clear();
            m_jobSystem = nullptr;
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
        if (!m_enableImporting)
            return 0;
        return m_importers.ImportAll(m_database);
    }

    size_t AssetManager::TickHotReload()
    {
        std::lock_guard lock(m_assetMutex);
        const auto now = std::chrono::steady_clock::now();
        if (!m_initialized || !m_enableHotReload || now < m_nextHotReloadPoll)
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
        return LoadCachedAsset(path, m_meshes, m_meshPathCache, MeshLoader::Load);
    }

    TextureHandle AssetManager::LoadTexture(const std::string& path)
    {
        return LoadCachedAsset(path, m_textures, m_texturePathCache, TextureLoader::Load);
    }

    ShaderHandle AssetManager::LoadShader(const std::string& path)
    {
        return LoadCachedAsset(path, m_shaders, m_shaderPathCache, ShaderLoader::Load);
    }

    ShaderTemplateHandle AssetManager::LoadShaderTemplate(const std::string& path)
    {
        return LoadCachedAsset(path, m_shaderTemplates, m_shaderTemplatePathCache, ShaderTemplateLoader::Load);
    }

    MaterialHandle AssetManager::LoadMaterial(const std::string& path)
    {
        return LoadCachedAsset(path, m_materials, m_materialPathCache, MaterialLoader::Load);
    }

    AnimationClipHandle AssetManager::LoadAnimationClip(const std::string& path)
    {
        return LoadCachedAsset(path, m_animationClips, m_animationClipPathCache, AnimationLoader::Load);
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

    const AssetRecord* AssetManager::ResolveReference(const AssetReference& reference, AssetType requiredType, std::string_view context) const
    {
        std::lock_guard lock(m_assetMutex);
        const AssetRecord* record = reference.IsValid() ? m_database.FindByGuid(reference.GetGuid()) : nullptr;
        if (!record && !reference.diagnosticPath.empty())
            record = m_database.FindByPath(reference.diagnosticPath);
        if (!record)
        {
            LOG_ERROR("AssetManager", "Missing asset reference '{}' for {}", reference.guid, context.empty() ? "unnamed consumer" : context);
            return nullptr;
        }

        const AssetType expectedType = reference.GetExpectedType();
        if ((expectedType != AssetType::Unknown && record->type != expectedType) || (requiredType != AssetType::Unknown && record->type != requiredType))
        {
            LOG_ERROR("AssetManager", "Asset reference '{}' type mismatch for {}: record={}, expected={}, required={}", record->guid.value, context.empty() ? "unnamed consumer" : context, AssetDatabase::AssetTypeName(record->type), AssetDatabase::AssetTypeName(expectedType), AssetDatabase::AssetTypeName(requiredType));
            return nullptr;
        }
        return record;
    }

    TextureHandle AssetManager::LoadTexture(const AssetReference& reference)
    {
        const AssetRecord* record = ResolveReference(reference, AssetType::Texture, "Texture");
        return record ? LoadTexture(record->guid) : TextureHandle::Invalid();
    }

    MeshHandle AssetManager::LoadMesh(const AssetReference& reference)
    {
        const AssetRecord* record = ResolveReference(reference, AssetType::Mesh, "Mesh");
        return record ? LoadMesh(record->guid) : MeshHandle::Invalid();
    }

    ShaderHandle AssetManager::LoadShader(const AssetReference& reference)
    {
        const AssetRecord* record = ResolveReference(reference, AssetType::ShaderSource, "Shader");
        return record ? LoadShader(record->guid) : ShaderHandle::Invalid();
    }

    MaterialHandle AssetManager::LoadMaterial(const AssetReference& reference)
    {
        const AssetRecord* record = ResolveReference(reference, AssetType::Material, "Material");
        return record ? LoadMaterial(record->guid) : MaterialHandle::Invalid();
    }

    ShaderTemplateHandle AssetManager::LoadShaderTemplate(const AssetReference& reference)
    {
        const AssetRecord* record = ResolveReference(reference, AssetType::ShaderTemplate, "ShaderTemplate");
        return record ? LoadShaderTemplate(record->guid) : ShaderTemplateHandle::Invalid();
    }

    AnimationClipHandle AssetManager::LoadAnimationClip(const AssetReference& reference)
    {
        const AssetRecord* record = ResolveReference(reference, AssetType::Mesh, "AnimationClip source");
        return record ? LoadAnimationClip(record->guid) : AnimationClipHandle::Invalid();
    }

    std::shared_future<TextureHandle> AssetManager::LoadTextureAsync(std::string path)
    {
        const std::string key = NormalizeCachePath(path);
        return LaunchAsync<TextureHandle>("Asset.LoadTexture",
                                          key,
                                          m_textureAsyncRequests,
                                          [this, path = std::move(path)]
                                          {
                                              const TextureHandle handle = LoadTexture(path);
                                              if (!handle.IsValid())
                                                  throw std::runtime_error("failed to load texture: " + path);
                                              return handle;
                                          });
    }

    std::shared_future<MeshHandle> AssetManager::LoadMeshAsync(std::string path)
    {
        const std::string key = NormalizeCachePath(path);
        return LaunchAsync<MeshHandle>("Asset.LoadMesh",
                                       key,
                                       m_meshAsyncRequests,
                                       [this, path = std::move(path)]
                                       {
                                           const MeshHandle handle = LoadMesh(path);
                                           if (!handle.IsValid())
                                               throw std::runtime_error("failed to load mesh: " + path);
                                           return handle;
                                       });
    }

    std::shared_future<ShaderHandle> AssetManager::LoadShaderAsync(std::string path)
    {
        const std::string key = NormalizeCachePath(path);
        return LaunchAsync<ShaderHandle>("Asset.LoadShader",
                                         key,
                                         m_shaderAsyncRequests,
                                         [this, path = std::move(path)]
                                         {
                                             const ShaderHandle handle = LoadShader(path);
                                             if (!handle.IsValid())
                                                 throw std::runtime_error("failed to load shader: " + path);
                                             return handle;
                                         });
    }

    std::shared_future<MaterialHandle> AssetManager::LoadMaterialAsync(std::string path)
    {
        const std::string key = NormalizeCachePath(path);
        return LaunchAsync<MaterialHandle>("Asset.LoadMaterial",
                                           key,
                                           m_materialAsyncRequests,
                                           [this, path = std::move(path)]
                                           {
                                               const MaterialHandle handle = LoadMaterial(path);
                                               if (!handle.IsValid())
                                                   throw std::runtime_error("failed to load material: " + path);
                                               return handle;
                                           });
    }

    bool AssetManager::Unload(TextureHandle handle)
    {
        std::string unloadedPath;
        const bool unloaded = UnloadFromCache(handle, m_textures, m_texturePathCache, &unloadedPath);
        if (unloaded)
            ClearAsyncRequest(m_textureAsyncRequests, unloadedPath);
        return unloaded;
    }

    bool AssetManager::Unload(MeshHandle handle)
    {
        std::string unloadedPath;
        const bool unloaded = UnloadFromCache(handle, m_meshes, m_meshPathCache, &unloadedPath);
        if (unloaded)
            ClearAsyncRequest(m_meshAsyncRequests, unloadedPath);
        return unloaded;
    }

    bool AssetManager::Unload(ShaderHandle handle)
    {
        std::string unloadedPath;
        const bool unloaded = UnloadFromCache(handle, m_shaders, m_shaderPathCache, &unloadedPath);
        if (unloaded)
            ClearAsyncRequest(m_shaderAsyncRequests, unloadedPath);
        return unloaded;
    }

    bool AssetManager::Unload(MaterialHandle handle)
    {
        std::string unloadedPath;
        const bool unloaded = UnloadFromCache(handle, m_materials, m_materialPathCache, &unloadedPath);
        if (unloaded)
            ClearAsyncRequest(m_materialAsyncRequests, unloadedPath);
        return unloaded;
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

    bool AssetManager::ScheduleAssetJob(std::string_view name, std::function<void()> function)
    {
        if (!m_jobSystem)
            return false;
        auto sharedFunction = std::make_shared<std::function<void()>>(std::move(function));
        const Jobs::JobHandle handle = m_jobSystem->Schedule(name, [sharedFunction] { (*sharedFunction)(); });
        if (!handle.IsValid())
            return false;
        if (m_jobSystem->GetState(handle) == Jobs::JobState::Cancelled)
        {
            m_jobSystem->Release(handle);
            return false;
        }
        return m_jobSystem->Detach(handle);
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

        if (!m_enableImporting)
            return record->importedPath.empty() ? record->sourcePath.string() : record->importedPath.string();

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
