#pragma once

#include "ChikaEngine/AssetHandle.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/AssetReference.hpp"
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

        const Asset::AssetReference& GetMaterialReference() const
        {
            return _materialReference;
        }
        const Asset::AssetReference& GetMeshReference() const
        {
            return _meshReference;
        }

        /** @brief 设置正式 Material 引用并使 Runtime Handle 失效。 */
        void SetMaterialReference(Asset::AssetReference reference)
        {
            _materialReference = std::move(reference);
            OnDirty();
        }

        /** @brief 设置正式 Mesh 引用并使 Runtime Handle 失效。 */
        void SetMeshReference(Asset::AssetReference reference)
        {
            _meshReference = std::move(reference);
            OnDirty();
        }

        /** @brief 兼容旧调用方；路径只作为迁移诊断，不是持久身份。 */
        void SetMaterialPath(const std::string& path)
        {
            SetMaterialReference(Asset::AssetReference::LegacyPath(path, Asset::AssetType::Material));
        }

        /** @brief 兼容旧调用方；路径只作为迁移诊断，不是持久身份。 */
        void SetMeshPath(const std::string& path)
        {
            SetMeshReference(Asset::AssetReference::LegacyPath(path, Asset::AssetType::Mesh));
        }

        Asset::MeshHandle GetMeshAsset() const
        {
            return _meshAsset;
        }
        Asset::MaterialHandle GetMaterialAsset() const
        {
            return _materialAsset;
        }

        bool NeedsAssetResolve() const
        {
            return _dirty;
        }

        void ResolveAssets(Asset::AssetManager & assetMgr);

      private:
        MFIELD()
        Asset::AssetReference _materialReference;

        MFIELD()
        Asset::AssetReference _meshReference;

        Asset::MeshHandle _meshAsset{};
        Asset::MaterialHandle _materialAsset{};

        bool _dirty = true;
    };
} // namespace ChikaEngine::Framework
