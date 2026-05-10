#include "ViewportPanel.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <imgui.h>

namespace ChikaEngine::Editor
{

    void ViewportPanel::Initialize(EditorContext* context)
    {
        _context = context;
    }

    void ViewportPanel::OnImGuiRender()
    {
        ImGui::Begin("Viewport Panel");
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin(GetName().c_str(), &_isActive, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // 更新交互状态
        if (_context)
        {
            _context->isViewportHovered = ImGui::IsWindowHovered();
            _context->isViewportFocused = ImGui::IsWindowFocused();
        }

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        uint32_t width = static_cast<uint32_t>(viewportSize.x);
        uint32_t height = static_cast<uint32_t>(viewportSize.y);

        auto renderer = _context->renderer;
        if (renderer && width > 0 && height > 0)
        {
            // 检测到 Resize，通知 Renderer 延迟重建
            if (width != renderer->GetViewportWidth() || height != renderer->GetViewportHeight())
            {
                renderer->RequestResize(width, height);
            }

            Render::TextureHandle offscreen = renderer->GetOffscreenTexture();
            if (offscreen.IsValid())
            {
                void* texID = renderer->GetRHIHandle()->GetImGuiTextureHandle(offscreen);
                LOG_DEBUG("Mother fucker", "Rendering Offscreen");
                ImGui::Image(texID, ImVec2{ viewportSize.x, viewportSize.y });
            }
        }

        ImGui::End();
        // ImGui::PopStyleVar();
    }

    void ViewportPanel::Tick(float deltaTime) {}
} // namespace ChikaEngine::Editor