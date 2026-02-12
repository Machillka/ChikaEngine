#include "ChikaEngine/PhysicsJoltBackend.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include "ChikaEngine/JoltLayer.h"
#include "Jolt/Core/Reference.h"
#include "Jolt/Core/Memory.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Physics/PhysicsSettings.h"
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
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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

    JPH::Ref<JPH::Shape> PhysicsJoltBackend::CreateShape(const ColliderShapeDesc& desc)
    {
        switch (desc.type)
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

        case ChikaEngine::Physics::RigidbodyShapes::Sphere:
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

        // 修改为默认参数——两层 : 动 / 静态
        _bpInterface = std::make_unique<JoltHelper::BitmaskBroadPhaseLayerInterface>();
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
        LOG_INFO("Physics backend", "Simulating...");
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

    PhysicsBodyHandle PhysicsJoltBackend::CreateBodyFromDesc(const PhysicsBodyCreateDesc& desc)
    {
        if (!_bodyInterface)
            return 0;

        using namespace JPH;
        JPH::Ref<JPH::Shape> shape = CreateShape(desc.shapeDesc);
        if (!shape)
            return 0;

        RVec3 pos(desc.position.x, desc.position.y, desc.position.z);
        Quat rot(desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w);

        BodyCreationSettings settings;
        settings.mPosition = pos;
        settings.mRotation = rot;
        settings.SetShape(shape);

        if (desc.motionType == MotionType::Static)
            settings.mMotionType = EMotionType::Static;
        else if (desc.motionType == MotionType::Kinematic)
            settings.mMotionType = EMotionType::Kinematic;
        else
            settings.mMotionType = EMotionType::Dynamic;

        // 计算编码后的 ObjectLayer
        settings.mObjectLayer = JoltHelper::GetJoltObjectLayer(desc.layerMask, desc.motionType);

        if (desc.motionType != MotionType::Static)
        {
            settings.mMassPropertiesOverride.mMass = desc.mass;
            settings.mAllowDynamicOrKinematic = true;

            // NOTE: 此处加上一点线形阻尼 防止乱飘
            settings.mLinearDamping = 0.05f;
            settings.mAngularDamping = 0.05f;
        }

        settings.mIsSensor = desc.isTrigger;
        settings.mFriction = desc.friction;
        settings.mRestitution = desc.restitution;
        settings.mMassPropertiesOverride.mMass = desc.mass;
        settings.mAllowDynamicOrKinematic = true;

        // 创建并且激活body
        BodyID id = _bodyInterface->CreateAndAddBody(settings, EActivation::Activate);
        if (id.IsInvalid())
        {
            LOG_WARN("Physics System", "Failed to create Physics Body");
            return 0;
        }

        LOG_INFO("Physics System", "Created Successfully ID = {}", id.GetIndexAndSequenceNumber());
        return id.GetIndexAndSequenceNumber();
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

    bool PhysicsJoltBackend::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit)
    {
        if (!_physicsSystem)
            return false;

        // 创建射线
        JPH::RVec3 start(origin.x, origin.y, origin.z);
        JPH::Vec3 dir(direction.x, direction.y, direction.z);

        // 包括距离
        dir = dir.Normalized() * maxDistance;

        JPH::RRayCast ray(start, dir);
        JPH::RayCastResult hit;

        // TODO: 加入层级信息
        const JPH::NarrowPhaseQuery& query = _physicsSystem->GetNarrowPhaseQuery();

        if (query.CastRay(ray, hit))
        {
            // 3. 获取详细信息
            JPH::BodyLockRead lock(_physicsSystem->GetBodyLockInterface(), hit.mBodyID);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();

                outHit.hasHit = true;
                outHit.bodyHandle = body.GetID().GetIndexAndSequenceNumber();

                // 转化到距离
                outHit.distance = hit.mFraction * maxDistance;

                // 计算世界空间击中点
                JPH::RVec3 hitPos = ray.GetPointOnRay(hit.mFraction);
                outHit.point = Math::Vector3(static_cast<float>(hitPos.GetX()), static_cast<float>(hitPos.GetY()), static_cast<float>(hitPos.GetZ()));

                // 计算世界空间法线
                JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPos);
                outHit.normal = Math::Vector3(normal.GetX(), normal.GetY(), normal.GetZ());

                return true;
            }
        }

        outHit.hasHit = false;
        return false;
    }
    void PhysicsJoltBackend::SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& pos, const Math::Quaternion& rot)
    {
        if (!_bodyInterface)
            return;

        JPH::BodyID id(handle);
        if (!_bodyInterface->IsAdded(id))
            return;

        JPH::RVec3 p(pos.x, pos.y, pos.z);
        JPH::Quat q(rot.x, rot.y, rot.z, rot.w);

        // 强制设置位置和旋转
        _bodyInterface->SetPositionAndRotation(id, p, q, JPH::EActivation::Activate);

        // 重置 防止按照瞬时速度飞走
        _bodyInterface->SetLinearVelocity(id, JPH::Vec3::sZero());
        _bodyInterface->SetAngularVelocity(id, JPH::Vec3::sZero());
    }
} // namespace ChikaEngine::Physics