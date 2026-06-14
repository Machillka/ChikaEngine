#pragma once

#include "ChikaEngine/RenderWorld.hpp"

namespace ChikaEngine::Render
{
    /**
     * @brief 控制 RenderWorld 调试线框生成；默认关闭以避免正常编辑与运行产生额外工作。
     */
    struct RenderDebugVisualizationOptions
    {
        bool drawAABBs = false;
        bool drawFrustums = false;
    };

    /**
     * @brief 从不可变 RenderWorld Snapshot 追加 AABB 与 Frustum Gizmo。
     *
     * 该路径只生成编辑器调试线段，不进入正式 RenderQueue、Batch 或帧渲染统计。
     */
    void AppendRenderWorldDebugGizmos(const RenderWorldSnapshot& snapshot, const RenderDebugVisualizationOptions& options);
} // namespace ChikaEngine::Render
