#include "ChikaEngine/GpuDrivenRenderGraph.hpp"

#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/RenderPassBuilder.hpp"

#include <algorithm>
#include <string>

namespace ChikaEngine::Render
{
    namespace
    {
        /** @brief Converts element count and stride into a non-zero buffer size for empty-scene graph tests. */
        uint64_t BufferSize(uint32_t count, uint64_t stride)
        {
            return std::max<uint64_t>(1u, static_cast<uint64_t>(count) * stride);
        }
    } // namespace

    GpuDrivenGraphHandles AddGpuDrivenBufferFlow(RenderGraph& graph, RenderGraphBlackboard& blackboard, const GpuDrivenGraphDesc& desc)
    {
        const BufferDesc instanceDesc{
            .size = BufferSize(desc.instanceCapacity, sizeof(GpuInstanceData)),
            .usage = RHI_BufferUsage::Storage | RHI_BufferUsage::TransferDst,
            .memoryUsage = MemoryUsage::GPU_Only,
        };
        const BufferDesc drawGroupDesc{
            .size = BufferSize(desc.drawGroupCapacity, sizeof(GpuDrawGroup)),
            .usage = RHI_BufferUsage::Storage | RHI_BufferUsage::TransferDst,
            .memoryUsage = MemoryUsage::GPU_Only,
        };
        const BufferDesc visibilityDesc{
            .size = BufferSize(desc.instanceCapacity, sizeof(uint32_t)),
            .usage = RHI_BufferUsage::Storage | RHI_BufferUsage::TransferDst,
            .memoryUsage = MemoryUsage::GPU_Only,
        };
        const BufferDesc visibleDesc{
            .size = BufferSize(desc.visibleCapacity, sizeof(uint32_t)),
            .usage = RHI_BufferUsage::Storage,
            .memoryUsage = MemoryUsage::GPU_Only,
        };
        const BufferDesc indirectDesc{
            .size = BufferSize(desc.drawGroupCapacity, sizeof(GpuIndexedIndirectCommand)),
            .usage = RHI_BufferUsage::Storage | RHI_BufferUsage::Indirect | RHI_BufferUsage::TransferDst,
            .memoryUsage = MemoryUsage::GPU_Only,
        };
        const BufferDesc indirectResetDesc{
            .size = BufferSize(desc.drawGroupCapacity, sizeof(GpuIndexedIndirectCommand)),
            .usage = RHI_BufferUsage::TransferSrc | RHI_BufferUsage::TransferDst,
            .memoryUsage = MemoryUsage::GPU_Only,
        };

        GpuDrivenGraphHandles handles{
            .instanceData = graph._RegisterBuffer(std::string(RenderGraphSemantic::GpuInstanceData), instanceDesc),
            .drawGroups = graph._RegisterBuffer(std::string(RenderGraphSemantic::GpuDrawGroups), drawGroupDesc),
            .visibilityFlags = graph._RegisterBuffer(std::string(RenderGraphSemantic::GpuVisibilityFlags), visibilityDesc),
            .visibleInstances = graph._RegisterBuffer(std::string(RenderGraphSemantic::GpuVisibleInstances), visibleDesc),
            .indirectArgs = graph.ImportBuffer(std::string(RenderGraphSemantic::GpuIndirectArgs), BufferHandle::Invalid(), indirectDesc),
            .indirectReset = graph._RegisterBuffer(std::string(RenderGraphSemantic::GpuIndirectReset), indirectResetDesc),
        };

        blackboard.SetBuffer(std::string(RenderGraphSemantic::GpuInstanceData), handles.instanceData);
        blackboard.SetBuffer(std::string(RenderGraphSemantic::GpuDrawGroups), handles.drawGroups);
        blackboard.SetBuffer(std::string(RenderGraphSemantic::GpuVisibilityFlags), handles.visibilityFlags);
        blackboard.SetBuffer(std::string(RenderGraphSemantic::GpuVisibleInstances), handles.visibleInstances);
        blackboard.SetBuffer(std::string(RenderGraphSemantic::GpuIndirectArgs), handles.indirectArgs);
        blackboard.SetBuffer(std::string(RenderGraphSemantic::GpuIndirectReset), handles.indirectReset);

        graph.AddCopyPass(
            "GPU Driven Upload",
            [handles](RGPassBuilder& builder)
            {
                builder.WriteBuffer(handles.instanceData, ResourceState::CopyDst);
                builder.WriteBuffer(handles.drawGroups, ResourceState::CopyDst);
                builder.WriteBuffer(handles.indirectReset, ResourceState::CopyDst);
            },
            [](IRHICommandList*, RenderGraph*) {},
            false);

        graph.AddCopyPass(
            "GPU Driven Reset",
            [handles](RGPassBuilder& builder)
            {
                builder.ReadBuffer(handles.indirectReset, ResourceState::CopySrc);
                builder.WriteBuffer(handles.indirectArgs, ResourceState::CopyDst);
            },
            [](IRHICommandList*, RenderGraph*) {},
            false);

        graph.AddComputePass(
            "GPU Driven Cull",
            [handles](RGPassBuilder& builder)
            {
                builder.ReadBuffer(handles.instanceData, ResourceState::StorageRead);
                builder.ReadBuffer(handles.drawGroups, ResourceState::StorageRead);
                builder.WriteBuffer(handles.visibilityFlags, ResourceState::StorageWrite);
                builder.WriteBuffer(handles.visibleInstances, ResourceState::StorageWrite);
                builder.WriteBuffer(handles.indirectArgs, ResourceState::StorageWrite);
            },
            [](IRHICommandList* commandList, RenderGraph*)
            {
                // The production compute shader is bound by the GPU-driven path once the material consumer exists.
                // This no-op keeps the graph contract testable without requiring a real GPU execution backend.
                (void)commandList;
            });

        if (desc.addGraphicsConsumer)
        {
            graph.AddCopyPass(
                "GPU Driven Graphics Consumer",
                [handles](RGPassBuilder& builder)
                {
                    builder.ReadBuffer(handles.instanceData, ResourceState::StorageRead);
                    builder.ReadBuffer(handles.visibleInstances, ResourceState::StorageRead);
                    builder.ReadBuffer(handles.indirectArgs, ResourceState::IndirectArgument);
                },
                [](IRHICommandList*, RenderGraph*) {},
                true);
        }

        return handles;
    }
} // namespace ChikaEngine::Render
