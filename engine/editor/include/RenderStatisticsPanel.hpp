#pragma once

#include "IEditorPanel.hpp"

namespace ChikaEngine::Editor
{
    /**
     * @brief 显示最近完成帧的稳定渲染提交统计。
     *
     * 面板只读取 Renderer 诊断数据，不修改渲染状态，确保性能观察不会改变被观察对象。
     */
    class RenderStatisticsPanel final : public IEditorPanel
    {
      public:
        /**
         * @brief 保存 Editor 上下文，使面板可以只读访问 Renderer 统计。
         */
        void Initialize(EditorContext* context) override;

        /**
         * @brief 保留统一面板更新入口；统计由 Renderer 负责采集，因此无需主动轮询。
         */
        void Tick(float deltaTime) override;

        /**
         * @brief 绘制最近完成帧的 RenderGraph 与 RHI 统计。
         */
        void OnImGuiRender() override;

        /**
         * @brief 返回稳定面板名称，供 Editor 窗口注册和持久化布局使用。
         */
        const std::string& GetName() const override
        {
            static const std::string name = "Render Statistics";
            return name;
        }
    };
} // namespace ChikaEngine::Editor
