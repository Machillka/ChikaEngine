// Shared GPU-driven layout contract.
// Keep this file in sync with ChikaEngine::Render::GpuInstanceData and GpuDrawGroup.

#ifndef CHIKA_GPU_DRIVEN_LAYOUT_GLSL
#define CHIKA_GPU_DRIVEN_LAYOUT_GLSL

#define CHIKA_GPU_DRIVEN_LAYOUT_VERSION 1u
#define CHIKA_GPU_DRIVEN_CULL_WORKGROUP_SIZE 64u
#define CHIKA_GPU_DRIVEN_INVALID_VISIBLE_INDEX 0xffffffffu

struct GpuInstanceData
{
    mat4 model;
    vec4 boundsCenterRadius;
    vec4 boundsExtentsFlags;
    uint drawGroupIndex;
    uint objectId;
    uint meshId;
    uint materialId;
};

struct GpuDrawGroup
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
    uint visibleOffset;
    uint visibleCount;
    uint pipelineId;
    uint meshId;
    uint materialId;
    uint reserved0;
    uint reserved1;
};

struct GpuIndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

#endif
