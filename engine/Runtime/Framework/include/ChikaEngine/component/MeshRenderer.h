#pragma once

#include "ChikaEngine/AssetHandle.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include <string>

namespace ChikaEngine::Framework
{
    MCLASS(MeshRenderer) : public Component
    {
        REFLECTION_BODY(MeshRenderer)
      public:
        explicit MeshRenderer(const std::string& meshPath, const std::string& materialPath);
        MeshRenderer();
        ~MeshRenderer() override = default;

        // 第一次必须重载
        void Awake() override
        {
            _dirty = true;
        }
        void OnDirty() override
        {
            _dirty = true;
        }

        const std::string& GetMaterialPath() const
        {
            return _materialPath;
        }
        const std::string& GetMeshPath() const
        {
            return _meshPath;
        }

        void SetMaterialPath(const std::string& path)
        {
            _materialPath = path;
            OnDirty();
        }

        void SetMeshPath(const std::string& path)
        {
            _meshPath = path;
            OnDirty();
        }

        Asset::MeshHandle GetMeshAsset() const
        {
            return _meshAsset;
        }
        Asset::MaterialHandle GetMaterialAsset() const
        {
            return _materialAsset;
        }

        void ResolveAssets(Asset::AssetManager & assetMgr);

      private:
        // 材质只需要记录位置, 办法交给老爹来想
        // 实际上是交付给用户修改, 修改 Path 即可
        MFIELD()
        std::string _materialPath;

        MFIELD()
        std::string _meshPath;

        Asset::MeshHandle _meshAsset{};
        Asset::MaterialHandle _materialAsset{};
        bool _dirty = true;
    };
} // namespace ChikaEngine::Framework