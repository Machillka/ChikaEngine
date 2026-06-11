#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/vector4.h"
#include "ChikaEngine/scene/scene.hpp"
#include <algorithm>

namespace ChikaEngine::Framework
{
    void Transform::Translate(const Math::Vector3& delta)
    {
        position += delta;
    }

    void Transform::Translate(float x, float y, float z)
    {
        Math::Vector3 delta = Math::Vector3(x, y, z);
        position += delta;
    }

    void Transform::Rotate(const Math::Quaternion& q)
    {
        rotation = (q * rotation).Normalized();
    }

    void Transform::Rotate(const Math::Vector3& eulerAngle)
    {
        rotation = Math::Quaternion::FromEuler(eulerAngle) * rotation;
        rotation = rotation.Normalized();
    }

    void Transform::Scale(const Math::Vector3& factor)
    {
        scale.x *= factor.x;
        scale.y *= factor.y;
        scale.z *= factor.z;
    }

    void Transform::Scale(float factor)
    {
        scale = scale * factor;
    }

    void Transform::Scale(float x, float y, float z)
    {
        scale.x *= x;
        scale.y *= y;
        scale.z *= z;
    }

    void Transform::LookAt(const Math::Vector3& target, const Math::Vector3& up)
    {
        Math::Vector3 forwardVec = target - GetWorldPosition();
        if (forwardVec.Length() < 1e-6f)
            return;
        Math::Vector3 forward = forwardVec.Normalized();
        SetWorldPositionAndRotation(GetWorldPosition(), Math::Quaternion::LookAtRotation(forward, up));
    }

    Math::Vector3 Transform::Forward() const
    {
        return GetWorldRotation().Rotate(Math::Vector3::back);
    }

    Math::Vector3 Transform::Right() const
    {
        return GetWorldRotation().Rotate(Math::Vector3::right);
    }

    Math::Vector3 Transform::Up() const
    {
        return GetWorldRotation().Rotate(Math::Vector3::up);
    }

    void Transform::TranslateLocal(const Math::Vector3& localDelta)
    {
        // Note: Forward in local is -Z
        Math::Vector3 worldDelta = rotation.Rotate(localDelta);
        position += worldDelta;
    }

    void Transform::TranslateLocal(float x, float y, float z)
    {
        TranslateLocal(Math::Vector3(x, y, z));
    }

    Math::Mat4 Transform::GetLocalMatrix() const
    {
        return Math::Mat4::TRSMatrix(position, rotation.Normalized(), scale);
    }

    Math::Mat4 Transform::GetWorldMat() const
    {
        return _parent ? _parent->GetWorldMat() * GetLocalMatrix() : GetLocalMatrix();
    }

    Math::Vector3 Transform::GetWorldPosition() const
    {
        const auto world = GetWorldMat() * Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        return { world.x, world.y, world.z };
    }

    Math::Quaternion Transform::GetWorldRotation() const
    {
        return _parent ? (_parent->GetWorldRotation() * rotation).Normalized() : rotation.Normalized();
    }

    Math::Vector3 Transform::GetWorldScale() const
    {
        return _parent ? _parent->GetWorldScale() * scale : scale;
    }

    void Transform::SetWorldPositionAndRotation(const Math::Vector3& worldPosition, const Math::Quaternion& worldRotation)
    {
        if (!_parent)
        {
            position = worldPosition;
            rotation = worldRotation.Normalized();
            return;
        }

        const auto localPosition = _parent->GetWorldMat().Inverse() * Math::Vector4(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
        position = { localPosition.x, localPosition.y, localPosition.z };

        const auto parentRotation = _parent->GetWorldRotation().Normalized();
        const Math::Quaternion inverseParent(-parentRotation.x, -parentRotation.y, -parentRotation.z, parentRotation.w);
        rotation = (inverseParent * worldRotation).Normalized();
    }

    bool Transform::IsDescendantOf(const Transform* candidate) const
    {
        for (auto* current = _parent; current; current = current->_parent)
        {
            if (current == candidate)
                return true;
        }
        return false;
    }

    void Transform::RemoveChild(Transform* child)
    {
        std::erase(_children, child);
    }

    bool Transform::SetParent(Transform* parent, bool keepWorldTransform)
    {
        if (parent == this || parent == _parent || (parent && parent->IsDescendantOf(this)))
            return parent == _parent;

        if (parent && GetOwner() && parent->GetOwner() && GetOwner()->GetScene() != parent->GetOwner()->GetScene())
            return false;

        const auto worldPosition = GetWorldPosition();
        const auto worldRotation = GetWorldRotation();
        const auto worldScale = GetWorldScale();

        if (_parent)
            _parent->RemoveChild(this);

        _parent = parent;
        _parentId = parent && parent->GetOwner() ? parent->GetOwner()->GetID() : Core::InvalidGameObjectID;

        if (_parent)
            _parent->_children.push_back(this);

        if (keepWorldTransform)
        {
            SetWorldPositionAndRotation(worldPosition, worldRotation);
            if (_parent)
            {
                const auto parentScale = _parent->GetWorldScale();
                scale = {
                    parentScale.x == 0.0f ? worldScale.x : worldScale.x / parentScale.x,
                    parentScale.y == 0.0f ? worldScale.y : worldScale.y / parentScale.y,
                    parentScale.z == 0.0f ? worldScale.z : worldScale.z / parentScale.z,
                };
            }
            else
            {
                scale = worldScale;
            }
        }

        if (GetOwner())
            GetOwner()->RefreshActiveInHierarchy();
        return true;
    }

    void Transform::ResolveHierarchy(Scene& scene)
    {
        if (!Core::IsValidGameObjectID(_parentId))
            return;

        auto* parentObject = scene.GetGameObject(_parentId);
        if (!parentObject || !parentObject->transform || !SetParent(parentObject->transform))
            _parentId = Core::InvalidGameObjectID;
    }

    void Transform::OnDestroy()
    {
        const auto children = _children;
        for (auto* child : children)
        {
            if (child)
                child->SetParent(nullptr, true);
        }
        SetParent(nullptr, true);
    }

    void Transform::OnGizmo() const {}
} // namespace ChikaEngine::Framework
