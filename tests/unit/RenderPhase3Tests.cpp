#include "ChikaEngine/RenderPreparation.hpp"
#include "ChikaEngine/RenderValidation.hpp"
#include "ChikaEngine/jobs/JobSystem.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    using namespace ChikaEngine;

    int g_failures = 0;

    /** @brief Records all contract failures so one run reports the complete Phase 3 regression surface. */
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    struct Fixture
    {
        std::shared_ptr<const Render::RenderWorldSnapshot> snapshot;
        Render::RenderResourceView resources;
        Render::RenderView view;
    };

    /** @brief Builds deterministic pointer-free identities while varying pass and visibility classifications. */
    Fixture BuildFixture(uint32_t objectCount)
    {
        auto snapshot = std::make_shared<Render::RenderWorldSnapshot>();
        snapshot->objects.reserve(objectCount);
        for (uint32_t index = 0; index < objectCount; ++index)
        {
            Render::RenderObjectFlags flags = Render::RenderObjectFlags::Visible | Render::RenderObjectFlags::CastShadow;
            if (index % 17u == 0)
                flags = flags | Render::RenderObjectFlags::Transparent;
            else if (index % 19u == 0)
                flags = flags | Render::RenderObjectFlags::Skinned;
            if (index % 23u == 0)
                flags = Render::RenderObjectFlags::CastShadow;

            Render::RenderObjectProxy proxy;
            proxy.mesh = Resource::MeshHandle::FromParts(index % 4u, 1);
            proxy.material = Resource::MaterialHandle::FromParts(index % 4u, 1);
            proxy.flags = flags;
            proxy.layerMask = index % 31u == 0 ? 0u : 0xFFFFFFFFu;
            proxy.bounds.center = { 0.0f, 0.0f, static_cast<float>(index % 8u) * -0.125f };
            snapshot->objects.push_back({
                .handle = Render::RenderObjectHandle::FromParts(index, 1),
                .proxy = std::move(proxy),
            });
        }

        std::vector<Resource::MeshHandle> meshes;
        std::vector<Render::RenderMaterialMetadata> materials;
        for (uint32_t index = 0; index < 4; ++index)
        {
            meshes.push_back(Resource::MeshHandle::FromParts(index, 1));
            materials.push_back({
                .handle = Resource::MaterialHandle::FromParts(index, 1),
                .forwardPipeline = Render::PipelineHandle::FromParts(index * 3u, 1),
                .gbufferPipeline = Render::PipelineHandle::FromParts(index * 3u + 1u, 1),
                .shadowPipeline = Render::PipelineHandle::FromParts(index * 3u + 2u, 1),
            });
        }
        Render::RenderView view{ .primary = true };
        return {
            .snapshot = std::move(snapshot),
            .resources = Render::RenderResourceView(std::move(meshes), std::move(materials)),
            .view = view,
        };
    }

    /** @brief Executes the correctness oracle with thresholds disabled only where a stage must be exercised. */
    Render::RenderPreparationResult Prepare(const Fixture& fixture, Render::RenderCpuMode mode, Jobs::JobSystem* jobs)
    {
        return Render::PrepareRenderData(fixture.snapshot,
                                         fixture.view,
                                         fixture.view,
                                         fixture.resources,
                                         jobs,
                                         {
                                             .mode = mode,
                                             .minimumObjects = 0,
                                             .visibilityGrainSize = 64,
                                             .packetGrainSize = 64,
                                             .sortMinimumPackets = 0,
                                             .sortGrainSize = 128,
                                         });
    }

    /** @brief Verifies category contiguity, complete accounting and snapshot ownership. */
    void TestSceneViewLayoutAndLifetime()
    {
        Fixture fixture = BuildFixture(128);
        auto scene = Render::RenderSceneView::Build(fixture.snapshot);
        fixture.snapshot.reset();
        const auto& classification = scene.GetClassification();
        Check(scene.GetSnapshot() != nullptr, "scene view owns its immutable snapshot");
        Check(scene.GetInstances().size() == 128, "scene view preserves every source object");
        Check(classification.staticOpaqueCount + classification.skinnedCount + classification.transparentCount + classification.invalidResourceCount == 128, "scene classification neither drops nor duplicates objects");
        Check(scene.GetStaticOpaqueInstances().size() == classification.staticOpaqueCount, "static opaque range is category-contiguous");
        for (const auto& instance : scene.GetStaticOpaqueInstances())
            Check(instance.instanceClass == Render::RenderInstanceClass::StaticOpaque, "static opaque span contains only static instances");
    }

    /** @brief Verifies pointer-independent hashes, field sensitivity and transparent depth quantization. */
    void TestValidationHashes()
    {
        Fixture fixture = BuildFixture(512);
        const auto first = Prepare(fixture, Render::RenderCpuMode::Serial, nullptr);
        const auto repeated = Prepare(fixture, Render::RenderCpuMode::Serial, nullptr);
        Check(first.diagnostics.hashes == repeated.diagnostics.hashes, "identical serial input produces identical hashes");

        auto reorderedSnapshot = std::make_shared<Render::RenderWorldSnapshot>(*fixture.snapshot);
        std::ranges::reverse(reorderedSnapshot->objects);
        Fixture reordered{ .snapshot = reorderedSnapshot, .resources = fixture.resources, .view = fixture.view };
        const auto reorderedResult = Prepare(reordered, Render::RenderCpuMode::Serial, nullptr);
        Check(first.diagnostics.hashes == reorderedResult.diagnostics.hashes, "source insertion order does not change sorted draw hashes");

        Render::RenderQueueSet queues;
        Render::RenderObjectSnapshot object{ .handle = Render::RenderObjectHandle::FromParts(7, 1) };
        queues.forwardTransparent.packets.push_back({ .object = &object, .pass = Render::RenderPassClass::ForwardTransparent, .viewDepth = 1.0f });
        const uint64_t nearHash = Render::HashRenderPackets(queues);
        queues.forwardTransparent.packets.front().viewDepth = 2.0f;
        Check(nearHash != Render::HashRenderPackets(queues), "transparent depth changes packet hash");
    }

    /** @brief Compares each supported worker topology against the exact serial visible/packet/batch oracle. */
    void TestWorkerEquivalence()
    {
        const Fixture fixture = BuildFixture(4'096);
        const auto serial = Prepare(fixture, Render::RenderCpuMode::Serial, nullptr);
        for (uint32_t workerCount : std::array{ 1u, 2u, 4u, 8u })
        {
            Jobs::JobSystem jobs;
            Check(jobs.Initialize({ .workerCount = workerCount, .reservedThreads = 0 }), "job system initializes for renderer equivalence test");
            const auto parallel = Prepare(fixture, Render::RenderCpuMode::Jobs, &jobs);
            Check(parallel.diagnostics.jobsUsed, "jobs mode executes object-level preparation");
            Check(parallel.diagnostics.hashes == serial.diagnostics.hashes, "worker topology preserves renderer validation hashes");
            Check(parallel.mainVisibility.visibleObjectCount == serial.mainVisibility.visibleObjectCount, "worker topology preserves visible count");
            Check(parallel.queues.forwardOpaque.batches.size() == serial.queues.forwardOpaque.batches.size(), "worker topology preserves opaque batches");
            jobs.Shutdown(Jobs::JobShutdownPolicy::Drain);
        }
    }

    /** @brief Verifies explicit fallback reasons instead of silently claiming parallel execution. */
    void TestFallbackContracts()
    {
        const Fixture fixture = BuildFixture(64);
        const auto unavailable = Render::PrepareRenderData(fixture.snapshot, fixture.view, fixture.view, fixture.resources, nullptr, { .mode = Render::RenderCpuMode::Jobs, .minimumObjects = 0 });
        Check(!unavailable.diagnostics.jobsUsed && unavailable.diagnostics.fallback == Render::RenderPreparationFallback::JobsUnavailable, "missing job system reports explicit fallback");

        Jobs::JobSystem jobs;
        Check(jobs.Initialize({ .workerCount = 2, .reservedThreads = 0 }), "job system initializes for threshold fallback test");
        const auto belowThreshold = Render::PrepareRenderData(fixture.snapshot, fixture.view, fixture.view, fixture.resources, &jobs, { .mode = Render::RenderCpuMode::Jobs, .minimumObjects = 2'048 });
        Check(!belowThreshold.diagnostics.jobsUsed && belowThreshold.diagnostics.fallback == Render::RenderPreparationFallback::BelowThreshold, "low object count reports threshold fallback");
        jobs.Shutdown(Jobs::JobShutdownPolicy::Drain);
    }
} // namespace

/** @brief Runs the complete CPU Renderer Phase 3 correctness gate without requiring an RHI device. */
int main()
{
    TestSceneViewLayoutAndLifetime();
    TestValidationHashes();
    TestWorkerEquivalence();
    TestFallbackContracts();
    return g_failures == 0 ? 0 : 1;
}
