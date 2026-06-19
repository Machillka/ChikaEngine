#pragma once

#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"

namespace ChikaEngine::Framework
{
    /**
     * @brief Scene 持有的最小方向光组件。
     *
     * 组件只描述 Gameplay 数据，RenderSubsystem 将其转换为 RenderLightProxy。
     */
    MCLASS(LightComponent) : public Component
    {
        REFLECTION_BODY(LightComponent)

      public:
        /** @brief 根据当前 Transform 构建方向光 Render Proxy。 */
        Render::RenderLightProxy BuildRenderLightProxy() const;

        MFIELD()
        Math::Vector3 color{ 1.0f, 1.0f, 1.0f };
        MFIELD()
        float intensity = 1.0f;
        MFIELD()
        bool castsShadow = true;
        MFIELD()
        uint32_t layerMask = 0xFFFFFFFFu;
    };
} // namespace ChikaEngine::Framework
