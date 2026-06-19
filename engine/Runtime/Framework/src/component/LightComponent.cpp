#include "ChikaEngine/component/LightComponent.hpp"

#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/mat4.h"
#include <cmath>

namespace ChikaEngine::Framework
{
    Render::RenderLightProxy LightComponent::BuildRenderLightProxy() const
    {
        const Transform* transform = GetOwner() ? GetOwner()->transform : nullptr;
        const Math::Vector3 position = transform ? transform->GetWorldPosition() : Math::Vector3(5.0f, 8.0f, 5.0f);
        const Math::Vector3 direction = transform ? transform->Forward() : Math::Vector3(0.5f, -1.0f, 0.3f).Normalized();
        const Math::Vector3 up = std::abs(direction.Dot(Math::Vector3::up)) > 0.99f ? Math::Vector3::right : Math::Vector3::up;

        Math::Mat4 projection = Math::Mat4::Orthographic(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 100.0f);
        projection(1, 1) *= -1.0f;
        return {
            .type = Render::RenderLightType::Directional,
            .position = position,
            .direction = direction,
            .color = color,
            .intensity = intensity,
            .viewProjection = projection * Math::Mat4::LookAt(position, position + direction, up),
            .layerMask = layerMask,
            .castsShadow = castsShadow,
        };
    }
} // namespace ChikaEngine::Framework
