#pragma once

#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"

namespace ChikaEngine::Framework
{
    /**
     * @brief 由 Scene Transform 驱动的 Runtime Camera。
     *
     * Editor View 不读取该组件；Standalone Game 使用 primary Camera 构建 RenderWorld 主视图。
     */
    MCLASS(CameraComponent) : public Component
    {
        REFLECTION_BODY(CameraComponent)

      public:
        /** @brief 根据当前 Transform 和视口宽高比构建无 Gameplay 指针的 RenderView。 */
        Render::RenderView BuildRenderView(float aspectRatio) const;

        MFIELD()
        bool primary = true;
        MFIELD()
        float fieldOfView = 45.0f;
        MFIELD()
        float nearClip = 0.1f;
        MFIELD()
        float farClip = 1000.0f;
        MFIELD()
        uint32_t layerMask = 0xFFFFFFFFu;
    };
} // namespace ChikaEngine::Framework
