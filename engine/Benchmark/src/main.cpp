#include "ChikaEngine/Application.hpp"
#include "ChikaEngine/benchmark/BenchmarkOptions.hpp"
#include "ChikaEngine/benchmark/BenchmarkRunner.hpp"
#include "ChikaEngine/benchmark/BenchmarkSceneFactory.hpp"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/jobs/JobSystem.hpp"
#include "ChikaEngine/RenderPath.hpp"
#include "ChikaEngine/scene/SceneManager.hpp"
#include "ChikaEngine/scene/scene.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

namespace
{
    using namespace ChikaEngine;

    /**
     * @brief Runs one deterministic benchmark through the production Application and EngineContext loop.
     *
     * The frontend owns workload and persistence policy while the engine remains unaware of benchmark code.
     */
    class BenchmarkApplication final : public Engine::Application
    {
      public:
        explicit BenchmarkApplication(Benchmark::BenchmarkOptions options) : m_options(std::move(options)) {}

      protected:
        /** @brief Disables non-essential development services and display pacing for comparable samples. */
        Engine::EngineContextCreateInfo CreateEngineContextInfo() const override
        {
            Engine::EngineContextCreateInfo createInfo;
            createInfo.window.title = "ChikaEngine Benchmark";
            createInfo.window.width = m_options.width;
            createInfo.window.height = m_options.height;
            createInfo.window.vSync = false;
            createInfo.window.visible = false;
            createInfo.enableInput = false;
            createInfo.enableScripting = false;
            createInfo.renderPipeline = Render::RenderPipelineMode::Forward;
            createInfo.runtimeMode = Project::RuntimeMode::DevelopmentGame;
            createInfo.contentRoot = "Assets";
            createInfo.createContentRoot = false;
            createInfo.scanAssets = true;
            createInfo.createMissingMeta = false;
            createInfo.importAssets = true;
            createInfo.enableHotReload = false;
            if (m_options.mode == Benchmark::BenchmarkMode::Gpu)
            {
                createInfo.renderPathMode = Render::RenderPathMode::GpuDriven;
                createInfo.renderCpuMode = Render::RenderCpuMode::Jobs;
            }
            else
            {
                createInfo.renderPathMode = m_options.mode == Benchmark::BenchmarkMode::Jobs ? Render::RenderPathMode::JobCpu : Render::RenderPathMode::SerialCpu;
                createInfo.renderCpuMode = m_options.mode == Benchmark::BenchmarkMode::Jobs ? Render::RenderCpuMode::Jobs : Render::RenderCpuMode::Serial;
            }
            createInfo.jobWorkerCount = m_options.workers;
            createInfo.reservedJobThreads = 0;
            createInfo.createDefaultScene = false;
            createInfo.useEditorView = false;
            return createInfo;
        }

        /** @brief Creates fixtures before warmup and freezes their identity into the result configuration. */
        void OnInitialize() override
        {
            auto* sceneManager = GetEngineContext().GetSceneManager();
            auto* assets = GetEngineContext().GetAssetManager();
            if (!sceneManager || !assets)
                throw std::runtime_error("Benchmark requires an initialized scene manager and asset manager");
            auto* scene = sceneManager->CreateScene("Benchmark", true);
            if (!scene)
                throw std::runtime_error("Failed to create the benchmark scene");

            const Benchmark::BenchmarkSceneBuildResult build = m_sceneFactory.Build(*scene, *assets, m_options.scene, m_options.seed, std::chrono::milliseconds(m_options.sceneBuildTimeoutMs));
            if (!build.success)
                throw std::runtime_error(build.error);
            if (!scene->StartPlayMode())
                throw std::runtime_error("Failed to enter benchmark play mode");

            Benchmark::BenchmarkResult result;
            result.build = Benchmark::CollectBenchmarkBuildMetadata();
            result.hardware = Benchmark::CollectBenchmarkHardwareMetadata();
            if (const auto* rhi = GetEngineContext().GetRenderer()->GetRHIHandle())
                result.hardware.gpuName = rhi->GetDeviceName();
            const Render::RenderPathMode requestedPath = m_options.mode == Benchmark::BenchmarkMode::Gpu ? Render::RenderPathMode::GpuDriven : (m_options.mode == Benchmark::BenchmarkMode::Jobs ? Render::RenderPathMode::JobCpu : Render::RenderPathMode::SerialCpu);
            const Render::RenderPathSelection pathSelection = Render::SelectRenderPath(GetEngineContext().GetRenderer()->GetRHIHandle()->GetCapabilities(),
                                                                                      {
                                                                                          .requested = requestedPath,
                                                                                          .jobSystemAvailable = GetEngineContext().GetJobSystem() != nullptr,
                                                                                          .gpuDrivenConsumerAvailable = true,
                                                                                      });
            result.configuration = {
                .scene = std::string(Benchmark::ToString(m_options.scene)),
                .requestedMode = std::string(Benchmark::ToString(m_options.mode)),
                .effectiveMode = std::string(Render::ToString(pathSelection.effective)),
                .workers = GetEngineContext().GetJobSystem() ? GetEngineContext().GetJobSystem()->GetWorkerCount() : 0,
                .warmupFrames = m_options.warmupFrames,
                .sampleFrames = m_options.sampleFrames,
                .flushFrames = m_options.flushFrames,
                .width = m_options.width,
                .height = m_options.height,
                .seed = m_options.seed,
                .sceneBuildTimeoutMs = m_options.sceneBuildTimeoutMs,
                .sceneConfigHash = build.sceneConfigHash,
                .vSync = false,
            };
            m_runner = std::make_unique<Benchmark::BenchmarkRunner>(std::move(result));
            std::cout << "Benchmark scene=" << Benchmark::ToString(m_options.scene) << " objects=" << build.objectCount << " hash=" << build.sceneConfigHash << '\n';
        }

        /** @brief Advances dynamic fixtures from an absolute frame index to avoid delta-time drift. */
        void OnUpdate(float) override
        {
            if (auto* scene = GetEngineContext().GetScene())
                m_sceneFactory.UpdateDynamicObjects(*scene, m_simulationFrame++);
        }

        /** @brief Converts renderer diagnostics into one runner observation after all frame work completed. */
        void OnFrameComplete(const Engine::ApplicationFrameMetrics& metrics) override
        {
            auto* renderer = GetEngineContext().GetRenderer();
            if (!renderer || !m_runner)
                throw std::runtime_error("Benchmark frame completed without renderer or runner");
            if (!renderer->IsFrameActive())
                return;

            double renderGraphCpuTimeMs = 0.0;
            for (const auto& pass : renderer->GetRenderGraphDebugSnapshot().passes)
                renderGraphCpuTimeMs += pass.cpuTimeMs;

            std::optional<Benchmark::BenchmarkFrameObservation::CompletedGpuFrame> completedGpuFrame;
            const auto& gpuTimings = renderer->GetRHIHandle()->GetPassGpuTimings();
            if (!gpuTimings.empty())
            {
                Benchmark::BenchmarkFrameObservation::CompletedGpuFrame completed{ .frameIndex = gpuTimings.front().frameIndex };
                for (const auto& pass : gpuTimings)
                {
                    if (pass.frameIndex != completed.frameIndex)
                        throw std::runtime_error("RHI returned GPU timings from multiple submitted frames");
                    if (std::isfinite(pass.gpuTimeMs) && pass.gpuTimeMs >= 0.0)
                        completed.gpuTimeMs += pass.gpuTimeMs;
                }
                completedGpuFrame = completed;
            }

            ChikaEngine::Jobs::JobSystemStatistics jobDelta;
            if (const auto* jobs = GetEngineContext().GetJobSystem())
            {
                const auto current = jobs->GetStatistics();
                jobDelta.queueWaitNanoseconds = current.queueWaitNanoseconds - m_previousJobStatistics.queueWaitNanoseconds;
                jobDelta.executionNanoseconds = current.executionNanoseconds - m_previousJobStatistics.executionNanoseconds;
                jobDelta.successfulSteals = current.successfulSteals - m_previousJobStatistics.successfulSteals;
                jobDelta.workerUtilization = current.workerUtilization;
                m_previousJobStatistics = current;
            }

            m_runner->ObserveFrame({
                .frameIndex = m_activeFrameIndex++,
                .frameTimeMs = static_cast<double>(metrics.deltaTime) * 1000.0,
                .engineTickCpuTimeMs = metrics.engineTickCpuTimeMs,
                .renderGraphCpuTimeMs = renderGraphCpuTimeMs,
                .renderStatistics = renderer->GetFrameStatistics(),
                .jobQueueWaitNanoseconds = jobDelta.queueWaitNanoseconds,
                .jobExecutionNanoseconds = jobDelta.executionNanoseconds,
                .successfulJobSteals = jobDelta.successfulSteals,
                .workerUtilization = jobDelta.workerUtilization,
                .completedGpuFrame = completedGpuFrame,
            });

            if (!m_runner->IsComplete())
                return;

            std::string error;
            if (!Benchmark::WriteBenchmarkResult(m_options.output, m_runner->GetResult(), error))
                throw std::runtime_error("Failed to write benchmark result: " + error);
            const auto& frame = m_runner->GetResult().aggregates.frameTime;
            std::cout << "Benchmark complete samples=" << frame.count << " frame_p50_ms=" << frame.p50 << " frame_p95_ms=" << frame.p95 << " output=" << m_options.output.string() << '\n';
            RequestExit();
        }

      private:
        Benchmark::BenchmarkOptions m_options;
        Benchmark::BenchmarkSceneFactory m_sceneFactory;
        std::unique_ptr<Benchmark::BenchmarkRunner> m_runner;
        uint64_t m_simulationFrame = 0;
        uint64_t m_activeFrameIndex = 0;
        ChikaEngine::Jobs::JobSystemStatistics m_previousJobStatistics;
    };
} // namespace

/** @brief Validates the CLI contract before constructing any window or graphics resources. */
int main(int argc, const char* const* argv)
{
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());
    const ChikaEngine::Benchmark::BenchmarkOptionsParseResult parsed = ChikaEngine::Benchmark::ParseBenchmarkOptions(argc, argv);
    if (!parsed.success)
    {
        std::cerr << parsed.error << "\n\n" << ChikaEngine::Benchmark::BenchmarkUsage();
        return 2;
    }
    if (parsed.options.showHelp)
    {
        std::cout << ChikaEngine::Benchmark::BenchmarkUsage();
        return 0;
    }
    if (parsed.options.mode == ChikaEngine::Benchmark::BenchmarkMode::Serial && parsed.options.workers != 1)
    {
        std::cerr << "Serial mode requires --workers 1.\n";
        return 3;
    }

    BenchmarkApplication application(parsed.options);
    return application.Run();
}
