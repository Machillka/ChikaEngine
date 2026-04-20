#include "ChikaEngine/component/MeshRenderer.h"
// #include "ChikaEngine/debug/log_macros.h"
namespace ChikaEngine::Framework
{
    MeshRenderer::MeshRenderer(const std::string& meshPath, const std::string& materialPath) : _dirty(true), _meshPath(meshPath), _materialPath(materialPath) {}

    // write down by copilot
    MeshRenderer::MeshRenderer() : _dirty(true) {}

    /*!
     * @brief 把 Path 从 Asset 中加载并且缓存 handle
     *
     * @param  assetMgr
     * @author Machillka (machillka2007@gmail.com)
     * @date 2026-04-06
     */
    void MeshRenderer::ResolveAssets(Asset::AssetManager& assetMgr)
    {
        // LOG_INFO("Mesh Renderer", "Resolveing");

        if (!_dirty)
            return;

        if (!_meshPath.empty())
            _meshAsset = assetMgr.LoadMesh(_meshPath);

        if (!_materialPath.empty())
            _materialAsset = assetMgr.LoadMaterial(_materialPath);

        _dirty = false;

        // LOG_INFO("Mesh Renderer", "Resolved successfully");
    }
} // namespace ChikaEngine::Framework