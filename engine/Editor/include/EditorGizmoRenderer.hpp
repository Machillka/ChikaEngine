#pragma once

#include <imgui.h>
#include "ChikaEngine/Camera.hpp"
#include "ChikaEngine/math/vector3.h"

namespace ChikaEngine::Editor
{

    class EditorGizmoRenderer
    {
      public:
        // 核心渲染接口
        static void Render(Render::Camera* camera, ImDrawList* drawList, const ImVec2& windowPos, const ImVec2& windowSize);

      private:
        // 内部坐标转换工具
        static bool WorldToScreen(const Math::Vector3& worldPos, const Math::Mat4& vpMatrix, const ImVec2& windowPos, const ImVec2& windowSize, ImVec2& outScreenPos);
    };
} // namespace ChikaEngine::Editor