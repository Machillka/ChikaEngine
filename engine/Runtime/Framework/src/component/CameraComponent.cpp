#include "ChikaEngine/component/CameraComponent.hpp"

#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/ChikaMath.h"

namespace ChikaEngine::Framework
{
    Render::RenderView CameraComponent::BuildRenderView(float aspectRatio) const
    {
        const Transform* transform = GetOwner() ? GetOwner()->transform : nullptr;
        return BuildRenderView(aspectRatio, transform ? transform->GetWorldMat() : Math::Mat4::Identity());
    }

    Render::RenderView CameraComponent::BuildRenderView(float aspectRatio, const Math::Mat4& worldTransform) const
    {
        const Math::Vector4 worldPosition = worldTransform * Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        const Math::Vector4 worldForward = worldTransform * Math::Vector4(0.0f, 0.0f, -1.0f, 0.0f);
        const Math::Vector4 worldUp = worldTransform * Math::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
        const Math::Vector3 position{ worldPosition.x, worldPosition.y, worldPosition.z };
        const Math::Vector3 forward = Math::Vector3(worldForward.x, worldForward.y, worldForward.z).Normalized();
        const Math::Vector3 up = Math::Vector3(worldUp.x, worldUp.y, worldUp.z).Normalized();

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
