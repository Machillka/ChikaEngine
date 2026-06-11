#pragma once

#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include <vector>

namespace ChikaEngine::Framework
{
    class Scene;

    MCLASS(Transform) : public Component
    {
        REFLECTION_BODY(Transform)
      public:
        Transform() = default;
        ~Transform() override = default;

        MFIELD()
        Math::Vector3 position{ 0.0f, 0.0f, 0.0f };
        MFIELD()
        Math::Quaternion rotation = Math::Quaternion::Identity();
        MFIELD()
        Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };

        // 世界空间对应操作
        MFUNCTION()
        void Translate(const Math::Vector3& delta);
        void Translate(float x, float y, float z);
        MFUNCTION()
        void Rotate(const Math::Quaternion& q);
        void Rotate(const Math::Vector3& eulerAngle);
        MFUNCTION()
        void Scale(const Math::Vector3& factor);
        void Scale(float x, float y, float z);
        void Scale(float factor);
        void LookAt(const Math::Vector3& target, const Math::Vector3& up = Math::Vector3::up);
        Math::Mat4 GetLocalMatrix() const;
        Math::Vector3 GetWorldPosition() const;
        Math::Quaternion GetWorldRotation() const;
        Math::Vector3 GetWorldScale() const;
        void SetWorldPositionAndRotation(const Math::Vector3& worldPosition, const Math::Quaternion& worldRotation);

        void ProcessLookDelta(float deltaX, float deltaY, bool constrainPitch = true);

        void TranslateLocal(const Math::Vector3& localDelta);
        void TranslateLocal(float x, float y, float z);

        // 在世界坐标的时候 GO对应方向
        Math::Vector3 Forward() const;
        Math::Vector3 Up() const;
        Math::Vector3 Right() const;

        Math::Mat4 GetWorldMat() const;

        bool SetParent(Transform * parent, bool keepWorldTransform = false);
        Transform* GetParent() const
        {
            return _parent;
        }
        const std::vector<Transform*>& GetChildren() const
        {
            return _children;
        }
        Core::GameObjectID GetParentId() const
        {
            return _parentId;
        }
        void ResolveHierarchy(Scene & scene);

        void OnGizmo() const override;
        void OnDestroy() override;

      private:
        friend class Scene;
        friend class Prefab;
        friend class GameObject;

        void SetSerializedParentId(Core::GameObjectID parentId)
        {
            _parentId = parentId;
        }
        void ResetHierarchyLinks()
        {
            _parent = nullptr;
            _children.clear();
        }
        bool IsDescendantOf(const Transform* candidate) const;
        void RemoveChild(Transform * child);

        Core::GameObjectID _parentId = Core::InvalidGameObjectID;
        Transform* _parent = nullptr;
        std::vector<Transform*> _children;
    };
} // namespace ChikaEngine::Framework
