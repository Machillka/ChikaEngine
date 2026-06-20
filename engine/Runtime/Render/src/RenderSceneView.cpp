#include "ChikaEngine/RenderSceneView.hpp"

#include "ChikaEngine/profiler/ProfilerMacros.hpp"

#include <array>
#include <iterator>

namespace ChikaEngine::Render
{
    namespace
    {
        /** @brief Applies one deterministic classification rule to every RenderObjectProxy. */
        RenderInstanceClass Classify(const RenderObjectProxy& proxy)
        {
            if (!proxy.mesh.IsValid() || !proxy.material.IsValid())
                return RenderInstanceClass::InvalidResource;
            if (HasFlag(proxy.flags, RenderObjectFlags::Transparent))
                return RenderInstanceClass::Transparent;
            if (HasFlag(proxy.flags, RenderObjectFlags::Skinned))
                return RenderInstanceClass::Skinned;
            return RenderInstanceClass::StaticOpaque;
        }

        /** @brief Copies only frame preparation fields and deliberately omits bone matrices. */
        RenderInstanceData MakeInstance(const RenderObjectSnapshot& object, uint32_t sourceIndex, RenderInstanceClass instanceClass)
        {
            return {
                .transform = object.proxy.transform,
                .bounds = object.proxy.bounds,
                .mesh = object.proxy.mesh,
                .material = object.proxy.material,
                .handle = object.handle,
                .layerMask = object.proxy.layerMask,
                .sourceIndex = sourceIndex,
                .flags = object.proxy.flags,
                .instanceClass = instanceClass,
            };
        }
    } // namespace

    RenderSceneView RenderSceneView::Build(std::shared_ptr<const RenderWorldSnapshot> snapshot)
    {
        CHIKA_PROFILE_SCOPE("Renderer.BuildSceneView");
        RenderSceneView result;
        result.m_snapshot = std::move(snapshot);
        if (!result.m_snapshot)
            return result;

        std::array<std::vector<RenderInstanceData>, 4> categories;
        for (auto& category : categories)
            category.reserve(result.m_snapshot->objects.size() / categories.size() + 1u);

        for (uint32_t sourceIndex = 0; sourceIndex < result.m_snapshot->objects.size(); ++sourceIndex)
        {
            const RenderObjectSnapshot& object = result.m_snapshot->objects[sourceIndex];
            const RenderInstanceClass instanceClass = Classify(object.proxy);
            categories[static_cast<size_t>(instanceClass)].push_back(MakeInstance(object, sourceIndex, instanceClass));
        }

        result.m_classification = {
            .staticOpaqueCount = static_cast<uint32_t>(categories[static_cast<size_t>(RenderInstanceClass::StaticOpaque)].size()),
            .skinnedCount = static_cast<uint32_t>(categories[static_cast<size_t>(RenderInstanceClass::Skinned)].size()),
            .transparentCount = static_cast<uint32_t>(categories[static_cast<size_t>(RenderInstanceClass::Transparent)].size()),
            .invalidResourceCount = static_cast<uint32_t>(categories[static_cast<size_t>(RenderInstanceClass::InvalidResource)].size()),
        };

        result.m_instances.reserve(result.m_snapshot->objects.size());
        for (auto& category : categories)
            result.m_instances.insert(result.m_instances.end(), std::make_move_iterator(category.begin()), std::make_move_iterator(category.end()));
        return result;
    }

    std::span<const RenderInstanceData> RenderSceneView::GetInstances() const
    {
        return m_instances;
    }

    std::span<const RenderInstanceData> RenderSceneView::GetStaticOpaqueInstances() const
    {
        return std::span(m_instances).first(m_classification.staticOpaqueCount);
    }

    const RenderObjectSnapshot* RenderSceneView::GetSourceObject(const RenderInstanceData& instance) const
    {
        if (!m_snapshot || instance.sourceIndex >= m_snapshot->objects.size())
            return nullptr;
        return &m_snapshot->objects[instance.sourceIndex];
    }

    const std::shared_ptr<const RenderWorldSnapshot>& RenderSceneView::GetSnapshot() const
    {
        return m_snapshot;
    }

    const RenderSceneClassification& RenderSceneView::GetClassification() const
    {
        return m_classification;
    }
} // namespace ChikaEngine::Render
