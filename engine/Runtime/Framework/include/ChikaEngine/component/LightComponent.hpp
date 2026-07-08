#pragma once

#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"

namespace ChikaEngine::Framework
{
    /**
     * @brief Scene 持有的最小光源组件。
     *
     * 组件只描述 Gameplay 数据，RenderSubsystem 将其转换为 RenderLightProxy。
     */
    MCLASS(LightComponent) : public Component
    {
        REFLECTION_BODY(LightComponent)

      public:
        /** @brief 根据当前 Transform 构建 Render Proxy。 */
        Render::RenderLightProxy BuildRenderLightProxy() const;

        /** @brief 0=Directional, 1=Point, 2=Spot。 */
        MFIELD()
        int lightType = 0;
        MFIELD()
        Math::Vector3 color{ 1.0f, 1.0f, 1.0f };
        MFIELD()
        float intensity = 1.0f;
        MFIELD()
        float range = 10.0f;
        MFIELD()
        float innerConeDegrees = 20.0f;
        MFIELD()
        float outerConeDegrees = 35.0f;
        MFIELD()
        bool castsShadow = true;
        MFIELD()
        uint32_t layerMask = 0xFFFFFFFFu;
    };
} // namespace ChikaEngine::Framework
