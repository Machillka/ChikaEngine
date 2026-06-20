#include "ChikaEngine/ParallelRenderVisibility.hpp"

#include "ChikaEngine/jobs/ParallelFor.hpp"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

#include <algorithm>

namespace ChikaEngine::Render
{
    VisibilityResult BuildVisibilityParallel(Jobs::JobSystem& jobs, const RenderSceneView& scene, const RenderView& view, bool shadowCastersOnly, const ParallelVisibilityConfig& config)
    {
        CHIKA_PROFILE_SCOPE("Renderer.Visibility.Parallel");
        const auto instances = scene.GetInstances();
        if (instances.size() < config.minimumObjects)
            return BuildVisibilitySerial(scene, view, shadowCastersOnly);

        const uint32_t count = static_cast<uint32_t>(instances.size());
        const uint32_t safeGrain = std::max(1u, config.grainSize);
        const uint32_t maximumChunks = std::max(1u, jobs.GetMaximumParallelChunks());
        const uint32_t chunkCapacity = std::min((count + safeGrain - 1u) / safeGrain, maximumChunks);
        std::vector<VisibilityResult> chunks(chunkCapacity);
        const ViewFrustum frustum = ViewFrustum::FromViewProjection(view.viewProjection);

        const Jobs::JobHandle parallel = Jobs::ParallelFor(jobs,
                                                           count,
                                                           safeGrain,
                                                           "Renderer.Visibility.Chunk",
                                                           [&](Jobs::ParallelForRange range)
                                                           {
                                                               VisibilityResult& output = chunks[range.chunkIndex];
                                                               output.visibleObjects.reserve(range.end - range.begin);
                                                               for (uint32_t index = range.begin; index < range.end; ++index)
                                                               {
                                                                   const RenderInstanceData& instance = instances[index];
                                                                   ++output.testedObjectCount;
                                                                   if (IsRenderInstanceVisible(instance, view, frustum, shadowCastersOnly))
                                                                   {
                                                                       if (const RenderObjectSnapshot* object = scene.GetSourceObject(instance))
                                                                       {
                                                                           output.visibleObjects.push_back(object);
                                                                           ++output.visibleObjectCount;
                                                                           continue;
                                                                       }
                                                                   }
                                                                   ++output.culledObjectCount;
                                                               }
                                                           });
        try
        {
            jobs.Wait(parallel);
        }
        catch (...)
        {
            jobs.Release(parallel);
            throw;
        }
        jobs.Release(parallel);

        VisibilityResult result;
        result.visibleObjects.reserve(instances.size());
        for (VisibilityResult& chunk : chunks)
        {
            result.testedObjectCount += chunk.testedObjectCount;
            result.visibleObjectCount += chunk.visibleObjectCount;
            result.culledObjectCount += chunk.culledObjectCount;
            result.visibleObjects.insert(result.visibleObjects.end(), chunk.visibleObjects.begin(), chunk.visibleObjects.end());
        }
        return result;
    }
} // namespace ChikaEngine::Render
