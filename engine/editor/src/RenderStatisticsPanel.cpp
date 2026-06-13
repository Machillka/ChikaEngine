#include "RenderStatisticsPanel.hpp"

#include "ChikaEngine/Renderer.hpp"
#include <imgui.h>

namespace ChikaEngine::Editor
{
    /**
     * @brief 保存 Editor 黑板引用，以便面板读取当前 Renderer。
     */
    void RenderStatisticsPanel::Initialize(EditorContext* context)
    {
        _context = context;
    }

    /**
     * @brief 保留统一 Panel Tick 接口。
     *
     * 当前统计由 Renderer 在帧执行后生成，因此面板不需要额外采样或缓存。
     */
    void RenderStatisticsPanel::Tick(float) {}

    /**
     * @brief 绘制最近完成帧的 RenderGraph 与 RHI 提交统计。
     *
     * Editor UI 在渲染帧之前构建，因此显示值对应上一帧，避免读取正在变化的统计数据。
     */
    void RenderStatisticsPanel::OnImGuiRender()
    {
        if (!ImGui::Begin(GetName().c_str(), &_isActive))
        {
            ImGui::End();
            return;
        }

        if (!_context || !_context->renderer)
        {
            ImGui::TextDisabled("Renderer unavailable");
            ImGui::End();
            return;
        }

        const auto& statistics = _context->renderer->GetFrameStatistics();
        ImGui::Text("Passes: %u", statistics.passCount);
        ImGui::Text("Draw Calls: %u", statistics.drawCallCount);
        ImGui::Text("Instances: %u", statistics.instanceCount);
        ImGui::Text("Pipeline Binds: %u", statistics.pipelineBindCount);
        ImGui::Text("Descriptor Writes: %u", statistics.descriptorUpdateCount);
        ImGui::Separator();
        ImGui::TextDisabled("ImGui backend draw calls are excluded.");
        ImGui::End();
    }
} // namespace ChikaEngine::Editor
