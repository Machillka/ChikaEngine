#include "ChikaEngine/component/LightComponent.hpp"

#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/ChikaMath.h"
#include "ChikaEngine/math/mat4.h"
#include <algorithm>
#include <cmath>

namespace ChikaEngine::Framework
{
    namespace
    {
        Render::RenderLightType ToRenderLightType(int value)
        {
            switch (std::clamp(value, 0, 2))
            {
            case 1:
                return Render::RenderLightType::Point;
            case 2:
                return Render::RenderLightType::Spot;
            default:
                return Render::RenderLightType::Directional;
            }
        }

        float ConeCosFromDegrees(float degrees)
        {
            return std::cos(degrees * Math::DEG2RAD);
        }
    } // namespace

    Render::RenderLightProxy LightComponent::BuildRenderLightProxy() const
    {
        const Transform* transform = GetOwner() ? GetOwner()->transform : nullptr;
        const Math::Vector3 position = transform ? transform->GetWorldPosition() : Math::Vector3(5.0f, 8.0f, 5.0f);
        const Math::Vector3 direction = transform ? transform->Forward() : Math::Vector3(0.5f, -1.0f, 0.3f).Normalized();
        const Math::Vector3 up = std::abs(direction.Dot(Math::Vector3::up)) > 0.99f ? Math::Vector3::right : Math::Vector3::up;
        const Render::RenderLightType type = ToRenderLightType(lightType);

        Math::Mat4 projection = Math::Mat4::Orthographic(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 100.0f);
        projection(1, 1) *= -1.0f;
        // Mat4::Orthographic emits OpenGL depth; shadow sampling expects Vulkan [0, 1] depth.
        projection(2, 2) *= 0.5f;
        projection(2, 3) = projection(2, 3) * 0.5f + 0.5f;

        constexpr float kDirectionalShadowDistance = 40.0f;
        const Math::Vector3 shadowEye = position - direction * kDirectionalShadowDistance;
        const float outerDegrees = std::clamp(std::max(outerConeDegrees, innerConeDegrees + 0.1f), 0.1f, 89.0f);
        const float innerDegrees = std::clamp(innerConeDegrees, 0.0f, outerDegrees - 0.1f);

        return {
            .type = type,
            .position = position,
            .direction = direction,
            .color = color,
            .intensity = intensity,
            .range = std::max(range, 0.001f),
            .innerConeCos = ConeCosFromDegrees(innerDegrees),
            .outerConeCos = ConeCosFromDegrees(outerDegrees),
            .viewProjection = projection * Math::Mat4::LookAt(shadowEye, position + direction * kDirectionalShadowDistance, up),
            .layerMask = layerMask,
            .castsShadow = castsShadow,
        };
    }
} // namespace ChikaEngine::Framework
