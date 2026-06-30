#pragma once

#include "ChikaEngine/RenderInstanceData.hpp"

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace ChikaEngine::Render
{
    struct RenderSceneClassification
    {
        uint32_t staticOpaqueCount = 0;
        uint32_t skinnedCount = 0;
        uint32_t transparentCount = 0;
        uint32_t invalidResourceCount = 0;
    };

    /** @brief Owns a snapshot and presents category-contiguous render instances for one frame. */
    class RenderSceneView
    {
      public:
        RenderSceneView() = default;

        /** @brief Builds category-contiguous AoS data without retaining Gameplay pointers. */
        static RenderSceneView Build(std::shared_ptr<const RenderWorldSnapshot> snapshot);

        /** @brief Returns all categories in their fixed contiguous order. */
        std::span<const RenderInstanceData> GetInstances() const;
        /** @brief Returns the leading static opaque category without allocating. */
        std::span<const RenderInstanceData> GetStaticOpaqueInstances() const;
        /** @brief Resolves a compact source index against the owned immutable snapshot. */
        const RenderObjectSnapshot* GetSourceObject(const RenderInstanceData& instance) const;
        /** @brief Exposes the owner reference used to keep packet object pointers valid. */
        const std::shared_ptr<const RenderWorldSnapshot>& GetSnapshot() const;
        /** @brief Returns complete non-overlapping category counts for diagnostics. */
        const RenderSceneClassification& GetClassification() const;

      private:
        std::shared_ptr<const RenderWorldSnapshot> m_snapshot;
        std::vector<RenderInstanceData> m_instances;
        RenderSceneClassification m_classification;
    };
} // namespace ChikaEngine::Render
