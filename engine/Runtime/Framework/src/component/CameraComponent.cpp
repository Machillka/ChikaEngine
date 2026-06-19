#include "ChikaEngine/component/CameraComponent.hpp"

#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/ChikaMath.h"

namespace ChikaEngine::Framework
{
    Render::RenderView CameraComponent::BuildRenderView(float aspectRatio) const
    {
        const Transform* transform = GetOwner() ? GetOwner()->transform : nullptr;
        const Math::Vector3 position = transform ? transform->GetWorldPosition() : Math::Vector3::zero;
        const Math::Vector3 forward = transform ? transform->Forward() : Math::Vector3::forward;
        const Math::Vector3 up = transform ? transform->Up() : Math::Vector3::up;

        Math::Mat4 projection = Math::Mat4::Perspective(fieldOfView * Math::DEG2RAD, aspectRatio, nearClip, farClip);
        projection(1, 1) *= -1.0f;
        const Math::Mat4 view = Math::Mat4::LookAt(position, position + forward, up);
        return {
            .view = view,
            .projection = projection,
            .viewProjection = projection * view,
            .position = position,
            .layerMask = layerMask,
            .primary = primary,
        };
    }
} // namespace ChikaEngine::Framework
