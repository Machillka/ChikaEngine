#include "physics/include/Collider.h"
#include "framework/component/Component.h"
#include "include/PhysicsSystem.h"
#include <cstdint>
namespace ChikaEngine::Framework
{
    void Collider::OnEnable()
    {
        Component::OnEnable();

        std::uint32_t layerIndex = static_cast<std::uint32_t>(layer);

        Physics::PhysicsSystem::Instance().SetLayerMask(layerIndex, collisionMask);
    }
            void Collider::OnDisable(){}
        void Collider::OnDestroy(){}
} // namespace ChikaEngine::Framework