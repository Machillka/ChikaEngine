#include "ChikaEngine/jobs/JobSystem.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct Options
    {
        uint32_t tasks = 128;
        uint32_t repeats = 3;
        std::filesystem::path output = "docs/dev/results/jobs/scheduler.json";
    };

    /** @brief Parses a deliberately small stable CLI used by reproducible Phase 2 benchmark runs. */
    Options ParseOptions(int argc, char** argv)
    {
        Options options;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--tasks" && index + 1 < argc)
                options.tasks = static_cast<uint32_t>(std::stoul(argv[++index]));
            else if (argument == "--repeats" && index + 1 < argc)
                options.repeats = static_cast<uint32_t>(std::stoul(argv[++index]));
            else if (argument == "--output" && index + 1 < argc)
                options.output = argv[++index];
            else
                throw std::invalid_argument("unknown or incomplete argument: " + argument);
        }
        options.tasks = std::max(1u, options.tasks);
        options.repeats = std::max(1u, options.repeats);
        return options;
    }

    /** @brief Produces CPU work with a known wall-time grain without introducing system sleeps. */
    void BusyWork(uint32_t microseconds)
    {
        const auto deadline = Clock::now() + std::chrono::microseconds(microseconds);
        while (Clock::now() < deadline)
            std::atomic_signal_fence(std::memory_order_seq_cst);
    }

    template <typename Function> double MeasureMilliseconds(Function&& function)
    {
        const auto begin = Clock::now();
        function();
        return std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    }

    /** @brief Returns the middle sample so sporadic desktop noise does not dominate comparison. */
    double Median(std::vector<double> samples)
    {
        std::ranges::sort(samples);
        return samples[samples.size() / 2];
    }

    /** @brief Serializes raw samples and scheduler counters instead of keeping only a headline speedup. */
    nlohmann::json MakeResult(std::string_view mode, uint32_t workers, uint32_t tasks, std::vector<double> samples, const ChikaEngine::Jobs::JobSystemStatistics* statistics = nullptr)
    {
        const double median = Median(samples);
        nlohmann::json result{
            { "mode", mode }, { "workers", workers }, { "samplesMs", samples }, { "medianMs", median }, { "throughputJobsPerSecond", static_cast<double>(tasks) / (median / 1'000.0) },
        };
        if (statistics)
        {
            result["statistics"] = {
                { "localPops", statistics->localPops }, { "injectionPops", statistics->injectionPops }, { "stealAttempts", statistics->stealAttempts }, { "successfulSteals", statistics->successfulSteals }, { "sleepCount", statistics->sleepCount }, { "queueWaitNanoseconds", statistics->queueWaitNanoseconds }, { "executionNanoseconds", statistics->executionNanoseconds }, { "workerUtilization", statistics->workerUtilization },
            };
        }
        return result;
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = ParseOptions(argc, argv);
        nlohmann::json output{
            { "schemaVersion", 1 }, { "buildType", CHIKA_JOB_BENCHMARK_BUILD_TYPE }, { "hardwareThreads", std::thread::hardware_concurrency() }, { "tasksPerRun", options.tasks }, { "repeats", options.repeats }, { "results", nlohmann::json::array() },
        };

        for (uint32_t taskMicroseconds : { 1u, 10u, 100u, 1'000u })
        {
            nlohmann::json workload{
                { "taskMicroseconds", taskMicroseconds },
                { "modes", nlohmann::json::array() },
            };
            std::vector<double> serialSamples;
            for (uint32_t repeat = 0; repeat < options.repeats; ++repeat)
                serialSamples.push_back(MeasureMilliseconds(
                    [&]
                    {
                        for (uint32_t task = 0; task < options.tasks; ++task)
                            BusyWork(taskMicroseconds);
                    }));
            const double serialMedian = Median(serialSamples);
            workload["modes"].push_back(MakeResult("serial", 1, options.tasks, serialSamples));

            std::vector<double> asyncSamples;
            for (uint32_t repeat = 0; repeat < options.repeats; ++repeat)
            {
                asyncSamples.push_back(MeasureMilliseconds(
                    [&]
                    {
                        std::vector<std::future<void>> futures;
                        futures.reserve(options.tasks);
                        for (uint32_t task = 0; task < options.tasks; ++task)
                            futures.push_back(std::async(std::launch::async, [=] { BusyWork(taskMicroseconds); }));
                        for (auto& future : futures)
                            future.get();
                    }));
            }
            nlohmann::json asyncResult = MakeResult("std-async", options.tasks, options.tasks, asyncSamples);
            asyncResult["speedupVsSerial"] = serialMedian / asyncResult["medianMs"].get<double>();
            workload["modes"].push_back(std::move(asyncResult));

            for (uint32_t workers : { 1u, 2u, 4u, 8u })
            {
                std::vector<double> samples;
                ChikaEngine::Jobs::JobSystemStatistics statistics;
                for (uint32_t repeat = 0; repeat < options.repeats; ++repeat)
                {
                    ChikaEngine::Jobs::JobSystem jobs;
                    jobs.Initialize({ .workerCount = workers, .jobCapacity = options.tasks + 16u, .injectionQueueCapacity = options.tasks + 16u });
                    samples.push_back(MeasureMilliseconds(
                        [&]
                        {
                            std::vector<ChikaEngine::Jobs::JobHandle> handles;
                            handles.reserve(options.tasks);
                            for (uint32_t task = 0; task < options.tasks; ++task)
                                handles.push_back(jobs.Schedule("Benchmark.Work", [=] { BusyWork(taskMicroseconds); }));
                            std::mutex completionMutex;
                            std::condition_variable completionCondition;
                            bool completed = false;
                            const ChikaEngine::Jobs::JobHandle completion = jobs.ScheduleAfter(handles,
                                                                                               "Benchmark.Complete",
                                                                                               [&]
                                                                                               {
                                                                                                   {
                                                                                                       std::lock_guard lock(completionMutex);
                                                                                                       completed = true;
                                                                                                   }
                                                                                                   completionCondition.notify_one();
                                                                                               });
                            for (const auto handle : handles)
                                jobs.Detach(handle);
                            {
                                std::unique_lock lock(completionMutex);
                                completionCondition.wait(lock, [&] { return completed; });
                            }
                            jobs.Wait(completion);
                            jobs.Release(completion);
                        }));
                    statistics = jobs.GetStatistics();
                    jobs.Shutdown();
                }
                nlohmann::json jobsResult = MakeResult("chika-jobs", workers, options.tasks, samples, &statistics);
                jobsResult["speedupVsSerial"] = serialMedian / jobsResult["medianMs"].get<double>();
                workload["modes"].push_back(std::move(jobsResult));
            }
            output["results"].push_back(std::move(workload));
        }

        if (const auto parent = options.output.parent_path(); !parent.empty())
            std::filesystem::create_directories(parent);
        std::ofstream stream(options.output, std::ios::binary | std::ios::trunc);
        stream << output.dump(2);
        if (!stream)
            throw std::runtime_error("failed to write job benchmark result");
        std::cout << "Job benchmark written to " << options.output << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Job benchmark failed: " << exception.what() << '\n';
        return 1;
    }
}
