#include "physics/include/PhysicsJoltBackend.h"

#include "debug/log_macros.h"
#include "framework/layer/layer.h"
#include "include/JoltLayer.h"
#include "include/PhysicsDescs.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include "Jolt/Core/Reference.h"
#include "Jolt/Core/Memory.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "math/vector3.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include "Jolt/Math/Quat.h"
#include "Jolt/Math/Real.h"
#include "Jolt/Physics/Body/BodyID.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace ChikaEngine::Physics
{
    // 把 jplt 碰撞事件 转化成 chika 的碰撞事件
    class PhysicsJoltBackend::JoltBackendContactListener : public JPH::ContactListener
    {
      public:
        JoltBackendContactListener(PhysicsJoltBackend* physicsBackend) : _physicsBackend(physicsBackend) {} // On Cillision Enter
        void OnContactAdded(const JPH::Body& b1, const JPH::Body& b2, const JPH::ContactManifold& manifold, JPH::ContactSettings& ioSettings) override
        {
            JPH::RVec3 p = manifold.GetWorldSpaceContactPointOn1(0);
            JPH::Vec3 n = manifold.mWorldSpaceNormal;
            float penetration = manifold.mPenetrationDepth;

            CollisionEvent e1;
            e1.selfRigidbodyHandle = b1.GetID().GetIndex();
            e1.oherRigidbodyHandle = b2.GetID().GetIndex();
            e1.contactPoint = ChikaEngine::Math::Vector3(p.GetX(), p.GetY(), p.GetZ());
            e1.contactNormal = ChikaEngine::Math::Vector3(n.GetX(), n.GetY(), n.GetZ());
            e1.impulse = 0.0f;

            CollisionEvent e2 = e1;
            e2.selfRigidbodyHandle = e1.oherRigidbodyHandle;
            e2.oherRigidbodyHandle = e1.selfRigidbodyHandle;
            e2.contactNormal = ChikaEngine::Math::Vector3(-e1.contactNormal.x, -e1.contactNormal.y, -e1.contactNormal.z);
            if (_physicsBackend)
            {
                _physicsBackend->PushEvent(e1);
                _physicsBackend->PushEvent(e2);
            }
        }
        // 持续碰撞
        void OnContactPersisted(const JPH::Body& b1, const JPH::Body& b2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {}
        // exit
        void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {}

      private:
        PhysicsJoltBackend* _physicsBackend;
    };

    PhysicsJoltBackend::PhysicsJoltBackend() : _listener(nullptr) {}
    PhysicsJoltBackend::~PhysicsJoltBackend()
    {
        Shutdown();
    }

    void PhysicsJoltBackend::PushEvent(const CollisionEvent& e)
    {
        std::lock_guard lock(_eventMutex);
        _eventQueue.push_back(e);
    }

    std::vector<CollisionEvent> PhysicsJoltBackend::PollCollisionEvents()
    {
        std::lock_guard lock(_eventMutex);
        std::vector<CollisionEvent> snap;
        snap.swap(_eventQueue);
        return snap;
    }

    JPH::Shape* PhysicsJoltBackend::CreateShape(const RigidbodyCreateDesc& desc)
    {
        switch (desc.shape)
        {
        case ChikaEngine::Physics::RigidbodyShapes::Box:
        {
            JPH::Vec3 half(desc.halfExtents.x, desc.halfExtents.y, desc.halfExtents.z);
            JPH::BoxShapeSettings settings(half);
            JPH::BoxShapeSettings::ShapeResult res = settings.Create();
            if (res.HasError())
            {
                LOG_ERROR("Physics", "Unable to create rigidbody");
                return nullptr;
            }
            JPH::Ref<JPH::Shape> shape = res.Get();
            return shape;
        }

        case ChikaEngine::Physics::RigidbodyShapes::Sphare:
        default:
        {
            JPH::SphereShapeSettings settings(desc.radius);
            JPH::SphereShapeSettings::ShapeResult res = settings.Create();
            if (res.HasError())
            {
                LOG_ERROR("Physics", "Unable to create rigidbody");
                return nullptr;
            }
            JPH::Ref<JPH::Shape> shape = res.Get();
            return shape;
        }
        }
    }

    bool PhysicsJoltBackend::Initialize(const PhysicsInitDesc& desc)
    {
        // 初始化 jph 设置
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        _tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        _jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers);
        _physicsSystem = std::make_unique<JPH::PhysicsSystem>();

        size_t N = static_cast<size_t>(Framework::GameObjectLayer::Count);
        {
            std::lock_guard lock(_maskMutex);
            // 设置为默认
            _masks.assign(N, ~Framework::LayerMask(0));
        }

        _bpInterface = std::make_unique<JoltHelper::BitmaskBroadPhaseLayerInterface>(1);
        _objVsBPFilter = std::make_unique<JoltHelper::BitmaskObjectVsBroadPhaseLayerFilter>();
        _pairFilter = std::make_unique<JoltHelper::BitmaskObjectLayerPairFilter>(_masks);

        _physicsSystem->Init(1024, 0, 1024, 1024, *_bpInterface, *_objVsBPFilter, *_pairFilter);

        _bodyInterface = &_physicsSystem->GetBodyInterface();
        _physicsSystem->SetGravity(JPH::Vec3(desc.gravity.x, desc.gravity.y, desc.gravity.z));

        _listener = std::make_unique<JoltBackendContactListener>(this);
        _physicsSystem->SetContactListener(_listener.get());

        LOG_INFO("JoltBackend", "Initialized");
        return true;
    }

    void PhysicsJoltBackend::Shutdown()
    {

        if (_physicsSystem)
        {
            if (_listener)
            {
                _physicsSystem->SetContactListener(nullptr);
                _listener.reset();
            }
        }

        if (_physicsSystem)
        {
            _physicsSystem.reset();
            _bodyInterface = nullptr;
        }

        if (_jobSystem)
        {
            _jobSystem.reset();
        }

        if (_tempAllocator)
        {
            _tempAllocator.reset();
        }

        if (JPH::Factory::sInstance)
        {
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }

        _pairFilter.reset();
        _objVsBPFilter.reset();
        _bpInterface.reset();

        {
            std::lock_guard lock(_eventMutex);
            _eventQueue.clear();
        }

        LOG_INFO("JoltBackend", "Shutdown complete");
    }

    void PhysicsJoltBackend::SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3 v)
    {
        std::lock_guard lock(_commandMutex);
        _velocityCommands.push_back(VelocityCommand{.handle = handle, .v = v});
    }

    void PhysicsJoltBackend::ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3 impulse)
    {
        std::lock_guard lock(_commandMutex);
        _impulseCommands.push_back(ImpulseCommand{.handle = handle, .impulse = impulse});
    }

    void PhysicsJoltBackend::SetLayerMask(std::uint32_t layerIndex, Framework::LayerMask mask)
    {
        {
            std::lock_guard lock(_maskMutex);
            if (layerIndex >= _masks.size())
            {
                _masks.resize(layerIndex + 1, ~Framework::LayerMask(0));
            }
            _masks[layerIndex] = mask;
        }

        if (_pairFilter)
        {
            if (auto* concrete = dynamic_cast<JoltHelper::BitmaskObjectLayerPairFilter*>(_pairFilter.get()))
            {
                concrete->SetMask(layerIndex, mask);
            }
        }
    }

    Framework::LayerMask PhysicsJoltBackend::GetLayerMask(std::uint32_t layerIndex) const
    {
        std::lock_guard lock(_maskMutex);
        if (layerIndex >= _masks.size())
            return ~Framework::LayerMask(0);
        return _masks[layerIndex];
    }
    void PhysicsJoltBackend::Simulate(float fixedDeltaTime)
    {
        if (!_physicsSystem)
            return;
        // TODO: 提供设置速度,以及impulse
        {
            std::lock_guard lk(_commandMutex);
            for (auto& velocityCmd : _velocityCommands)
            {
                JPH::BodyID id(velocityCmd.handle);
                if (_bodyInterface->IsAdded(id))
                {
                    _bodyInterface->SetLinearVelocity(id, JPH::Vec3(velocityCmd.v.x, velocityCmd.v.y, velocityCmd.v.z));
                }
            }
            _velocityCommands.clear();
            for (auto& impulseCmd : _impulseCommands)
            {
                JPH::BodyID id(impulseCmd.handle);
                if (_bodyInterface->IsAdded(id))
                {
                    _bodyInterface->AddImpulse(id, JPH::Vec3(impulseCmd.impulse.x, impulseCmd.impulse.y, impulseCmd.impulse.z));
                }
            }
            _impulseCommands.clear();
        }

        _physicsSystem->Update(fixedDeltaTime, 1, _tempAllocator.get(), _jobSystem.get());
    }

    PhysicsBodyHandle PhysicsJoltBackend::CreateBodyFromDesc(const RigidbodyCreateDesc& desc)
    {
        if (!_bodyInterface)
            return 0;

        using namespace JPH;
        Shape* shape = CreateShape(desc);
        if (!shape)
            return 0;

        RVec3 pos(desc.transform.pos.x, desc.transform.pos.y, desc.transform.pos.z);
        Quat rot(desc.transform.rot.x, desc.transform.rot.y, desc.transform.rot.z, desc.transform.rot.w);
        BodyCreationSettings settings;
        settings.SetShape(Ref<Shape>(shape));
        settings.mMotionType = desc.isKinematic ? EMotionType::Kinematic : EMotionType::Dynamic;
        settings.mMassPropertiesOverride.mMass = desc.mass;
        settings.mAllowDynamicOrKinematic = true;

        BodyID id = _bodyInterface->CreateAndAddBody(settings, EActivation::Activate);
        if (id.IsInvalid())
            return 0;
        return id.GetIndex();
    }

    void PhysicsJoltBackend::DestroyPhysicsBody(PhysicsBodyHandle handle)
    {
        if (!_bodyInterface)
            return;
        JPH::BodyID id(handle);
        if (_bodyInterface->IsAdded(id))
        {
            _bodyInterface->RemoveBody(id);
        }
    }

    bool PhysicsJoltBackend::TrySyncTransform(PhysicsBodyHandle handle, PhysicsTransform& trans)
    {
        if (!_bodyInterface)
            return false;
        JPH::BodyID id(handle);
        if (!_bodyInterface->IsAdded(id))
            return false;

        JPH::RVec3 p = _bodyInterface->GetPosition(id);
        JPH::Quat r = _bodyInterface->GetRotation(id);

        trans.pos = Math::Vector3(static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ()));
        trans.rot = Math::Quaternion(static_cast<float>(r.GetX()), static_cast<float>(r.GetY()), static_cast<float>(r.GetZ()), static_cast<float>(r.GetW()));

        return true;
    }

    bool PhysicsJoltBackend::HasRigidbody(PhysicsBodyHandle handle)
    {
        if (!_bodyInterface)
            return false;
        JPH::BodyID id(handle);
        if (!_bodyInterface->IsAdded(id))
            return false;
        return true;
    }

} // namespace ChikaEngine::Physics