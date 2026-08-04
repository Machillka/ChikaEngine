#include "RenderStatisticsPanel.hpp"

#include "ChikaEngine/RenderPath.hpp"
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
        ImGui::Text("Visible / Culled: %u / %u", statistics.visibleObjectCount, statistics.culledObjectCount);
        ImGui::Text("Packets / Batches: %u / %u", statistics.packetCount, statistics.batchCount);
        ImGui::Text("Instanced Batches: %u", statistics.instancedBatchCount);
        if (ImGui::CollapsingHeader("Render Path", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const char* pathItems[] = { "Serial CPU", "Job CPU", "GPU Driven" };
            int selectedPath = static_cast<int>(_context->renderer->GetRequestedRenderPathMode());
            if (ImGui::Combo("Requested Path", &selectedPath, pathItems, 3))
                _context->renderer->SetRenderPathMode(static_cast<Render::RenderPathMode>(selectedPath));
            const auto requested = static_cast<Render::RenderPathMode>(statistics.requestedRenderPath);
            const auto effective = static_cast<Render::RenderPathMode>(statistics.effectiveRenderPath);
            const auto fallback = static_cast<Render::RenderPathFallbackReason>(statistics.renderPathFallback);
            ImGui::Text("Requested: %s", Render::ToString(requested).data());
            ImGui::Text("Effective: %s", Render::ToString(effective).data());
            ImGui::Text("Fallback: %s", Render::ToString(fallback).data());
            ImGui::Text("GPU-driven Instances / Visible: %u / %u", statistics.gpuDrivenInstanceCount, statistics.gpuDrivenVisibleCount);
            ImGui::Text("GPU-driven Groups / Indirect: %u / %u", statistics.gpuDrivenDrawGroupCount, statistics.gpuDrivenIndirectCommandCount);
            ImGui::Text("GPU-driven Oracle: compared=%u matched=%u missing=%u extra=%u",
                        statistics.gpuDrivenValidationCompared,
                        statistics.gpuDrivenValidationMatched,
                        statistics.gpuDrivenValidationMissingCount,
                        statistics.gpuDrivenValidationExtraCount);
        }
        if (ImGui::CollapsingHeader("Render Quality", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& settings = _context->renderer->GetSettings();
            float exposure = settings.postProcess.exposure;
            float ambientIntensity = settings.ambientIntensity;
            bool bloomEnabled = settings.postProcess.bloomEnabled;
            bool fxaaEnabled = settings.postProcess.fxaaEnabled;
            if (ImGui::SliderFloat("Exposure", &exposure, 0.05f, 4.0f))
                _context->renderer->SetExposure(exposure);
            if (ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f))
                _context->renderer->SetAmbientIntensity(ambientIntensity);
            if (ImGui::Checkbox("Bloom", &bloomEnabled))
                _context->renderer->SetBloomEnabled(bloomEnabled);
            if (ImGui::Checkbox("FXAA", &fxaaEnabled))
                _context->renderer->SetFXAAEnabled(fxaaEnabled);
            ImGui::TextDisabled("Shadow: %u px, PCF radius %u", settings.shadows.resolution, settings.shadows.pcfRadius);
        }
        ImGui::Separator();
        const auto& graph = _context->renderer->GetRenderGraphDebugSnapshot();
        if (!graph.compileErrors.empty() && ImGui::CollapsingHeader("Compile Errors", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const std::string& error : graph.compileErrors)
                ImGui::TextWrapped("%s", error.c_str());
        }
        if (ImGui::CollapsingHeader("Pass DAG", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const auto& pass : graph.passes)
            {
                ImGui::BulletText("%s  CPU %.3f ms  GPU %.3f ms", pass.name.c_str(), pass.cpuTimeMs, pass.gpuTimeMs);
                if (!pass.dependencies.empty())
                {
                    ImGui::Indent();
                    ImGui::TextDisabled("Depends on:");
                    ImGui::SameLine();
                    for (size_t index = 0; index < pass.dependencies.size(); ++index)
                    {
                        if (index > 0)
                            ImGui::SameLine(0.0f, 4.0f);
                        ImGui::TextDisabled("%s%s", index > 0 ? "," : "", pass.dependencies[index].c_str());
                    }
                    ImGui::Unindent();
                }
            }
        }
        if (ImGui::CollapsingHeader("Resource Lifetimes"))
        {
            for (const auto& resource : graph.resources)
                ImGui::BulletText("%s [%s] %u..%u%s", resource.name.c_str(), resource.kind.c_str(), resource.firstUsePass, resource.lastUsePass, resource.imported ? " imported" : "");
        }
        if (ImGui::CollapsingHeader("Barriers"))
        {
            for (const auto& barrier : graph.barriers)
                ImGui::BulletText("%s: %s (%u -> %u)", barrier.pass.c_str(), barrier.resource.c_str(), static_cast<uint32_t>(barrier.before), static_cast<uint32_t>(barrier.after));
        }
        ImGui::Separator();
        ImGui::TextDisabled("ImGui backend draw calls are excluded.");
        ImGui::End();
    }
} // namespace ChikaEngine::Editor
