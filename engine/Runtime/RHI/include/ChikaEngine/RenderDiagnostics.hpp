#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ChikaEngine::Render
{
    /**
     * @brief 保存最近一帧可稳定比较的渲染提交统计。
     *
     * 统计只覆盖 ChikaEngine 显式录制的 RenderGraph Pass 和 RHI 命令。
     * 外部 Overlay 后端内部生成的 Draw Call 不计入，避免扩展实现细节污染引擎基线。
     */
    struct RenderFrameStatistics
    {
        uint32_t passCount = 0;
        uint32_t drawCallCount = 0;
        uint32_t instanceCount = 0;
        uint32_t pipelineBindCount = 0;
        uint32_t descriptorUpdateCount = 0;
        uint32_t visibleObjectCount = 0;
        uint32_t culledObjectCount = 0;
        uint32_t packetCount = 0;
        uint32_t batchCount = 0;
        uint32_t instancedBatchCount = 0;

        /**
         * @brief 清空统计，以便 RHI 在每个新帧开始时重新计数。
         *
         * 使用完整值重置而不是逐字段维护，确保后续新增统计项不会遗漏初始化。
         */
        void Reset()
        {
            *this = {};
        }
    };

    struct RenderPassGpuTiming
    {
        std::string name;
        double gpuTimeMs = 0.0;
        uint64_t frameIndex = 0;
        uint32_t queueId = 0;
        bool valid = true;
    };
} // namespace ChikaEngine::Render
