#include "ChikaEngine/IPhysicsBackend.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsEvents.hpp"
#include "ChikaEngine/PhysicsHandles.hpp"
#include "ChikaEngine/PhysicsCommandBuffer.hpp"
#include "ChikaEngine/PhysicsRuntime.hpp"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/PhysicsSystem.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>

namespace
{
    namespace Physics = ChikaEngine::Physics;

    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    bool NearlyEqual(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    Physics::PhysicsBodyCreateDesc MakeStaticBox(ChikaEngine::Core::GameObjectID ownerId)
    {
        Physics::PhysicsBodyCreateDesc desc;
        desc.ownerId = ownerId;
        desc.motionType = Physics::MotionType::Static;
        desc.shapeDesc.type = Physics::ColliderShapeType::Box;
        return desc;
    }

    void TestStrongHandleContract()
    {
        static_assert(!std::is_same_v<Physics::PhysicsBodyHandle, Physics::PhysicsColliderHandle>);
        static_assert(std::is_trivially_copyable_v<Physics::PhysicsBodyHandle>);
        static_assert(std::is_trivially_copyable_v<Physics::RawContactPacket>);
        static_assert(std::is_trivially_copyable_v<Physics::PhysicsPairEvent>);
        static_assert(!std::is_constructible_v<Physics::PhysicsBodyHandle, std::uint64_t>);
        static_assert(!std::is_convertible_v<std::uint64_t, Physics::PhysicsBodyHandle>);
        static_assert(!std::is_same_v<Physics::PhysicsBodyHandle, Physics::PhysicsBackendBodyToken>);

        constexpr Physics::PhysicsBodyHandle invalid;
        constexpr Physics::PhysicsBodyHandle handle = Physics::PhysicsBodyHandle::FromParts(7, 9);
        static_assert(!invalid.IsValid());
        static_assert(handle.IsValid());
        static_assert(handle.Index() == 7);
        static_assert(handle.Generation() == 9);
        static_assert(Physics::PhysicsBodyHandle::FromValue(handle.Value()) == handle);
        static_assert(!Physics::PhysicsBodyHandle::FromParts(7, 0).IsValid());
        static_assert(!Physics::PhysicsBodyHandle::FromValue(7).IsValid());

        std::unordered_set<Physics::PhysicsBodyHandle, Physics::PhysicsHandleHash> handles;
        handles.insert(handle);
        handles.insert(Physics::PhysicsBodyHandle::FromParts(8, 9));
        Check(handles.size() == 2, "physics handles support typed hashing without exposing backend ids");
        Check(!Physics::PhysicsBodyHandle::Invalid(), "body invalid sentinel converts to false");
        Check(!Physics::PhysicsColliderHandle::Invalid(), "collider invalid sentinel converts to false");
    }

    void TestDefaultContract()
    {
        const Physics::PhysicsSystemDesc systemDesc;
        Check(systemDesc.backendType == Physics::PhysicsBackendType::Jolt, "Jolt is the explicit default backend");
        Check(NearlyEqual(systemDesc.initDesc.gravity.x, 0.0f) && NearlyEqual(systemDesc.initDesc.gravity.y, -9.81f) && NearlyEqual(systemDesc.initDesc.gravity.z, 0.0f), "default gravity uses meters per second squared with Y-up");
        Check(systemDesc.initDesc.workerThreadCount == -1, "default worker count delegates sizing to the backend");
        Check(systemDesc.commandQueueCapacity == 4096, "default command queue capacity supports lifecycle bursts");

        const Physics::PhysicsBodyCreateDesc bodyDesc;
        Check(bodyDesc.shapeDesc.type == Physics::ColliderShapeType::Box, "default collider shape is Box");
        Check(bodyDesc.motionType == Physics::MotionType::Dynamic, "default body motion is Dynamic");
        Check(bodyDesc.layer == 0 && bodyDesc.collisionMask == Physics::PHYSICS_LAYER_MASK_ALL, "default body participates in the full collision mask");

        const Physics::RawContactPacket rawContact;
        const Physics::PhysicsPairEvent pairEvent;
        const Physics::RaycastHit hit;
        const Physics::PhysicsVelocityCommand velocity;
        const Physics::PhysicsImpulseCommand impulse;
        Check(!rawContact.bodyA && !rawContact.bodyB && rawContact.removalState == Physics::RawContactRemovalState::NotApplicable, "raw contact packets default to invalid strong handles and no removal classification");
        Check(!pairEvent.pair.bodyA && !pairEvent.pair.bodyB && !pairEvent.hasContactData && pairEvent.terminationReason == Physics::PhysicsPairTerminationReason::None, "pair events default to an empty canonical contract");
        Check(!hit.bodyHandle && !hit.hasHit, "raycast results default to a miss with an invalid handle");
        Check(!velocity.target.handle && !impulse.target.handle, "deferred body commands default to invalid handles");
    }

    void TestFailedInitializationIsSafe()
    {
        Physics::PhysicsSystemDesc unsupportedDesc;
        unsupportedDesc.backendType = Physics::PhysicsBackendType::None;
        Physics::PhysicsScene unsupported(unsupportedDesc);
        Check(!unsupported.IsInitialized(), "None backend does not produce an initialized Scene");
        Check(unsupported.GetInitializationResult().status == Physics::PhysicsStatus::UnsupportedBackend, "None backend reports UnsupportedBackend");

        Physics::RaycastHit hit;
        hit.hasHit = true;
        hit.bodyHandle = Physics::PhysicsBodyHandle::FromParts(1, 1);
        Check(!unsupported.Raycast({ 0, 1, 0 }, { 0, -1, 0 }, 10.0f, hit), "raycast is safe after initialization failure");
        Check(!hit.hasHit && !hit.bodyHandle, "failed Scene raycast resets its output");
        Check(!unsupported.CreateBodyImmediate({}), "body creation fails safely on an uninitialized Scene");
        Check(!unsupported.SetBodyTransform(Physics::PhysicsBodyHandle::FromParts(1, 1), {}, { 0, 0, 0, 1 }), "body mutation is safe after initialization failure");
        Check(unsupported.PollTransform().empty(), "transform polling is empty after initialization failure");
        Check(!unsupported.GetCapabilities().boxShape, "failed Scene exposes no backend capabilities");
        unsupported.Tick(1.0f / 60.0f);
        unsupported.Shutdown();
        unsupported.Shutdown();
        Check(unsupported.GetInitializationResult().status == Physics::PhysicsStatus::NotInitialized, "repeated Shutdown leaves a deterministic NotInitialized status");

        const Physics::PhysicsRuntime::Statistics before = Physics::PhysicsRuntime::GetStatistics();
        Physics::PhysicsSystemDesc invalidDesc;
        invalidDesc.initDesc.gravity.x = std::numeric_limits<float>::quiet_NaN();
        {
            Physics::PhysicsScene invalid(invalidDesc);
            Check(!invalid.IsInitialized(), "non-finite gravity rejects Jolt initialization");
            Check(invalid.GetInitializationResult().status == Physics::PhysicsStatus::InvalidArgument, "invalid initialization reports InvalidArgument");
            invalid.Tick(1.0f / 60.0f);
            invalid.Shutdown();
        }
        const Physics::PhysicsRuntime::Statistics after = Physics::PhysicsRuntime::GetStatistics();
        Check(after.activeLeases == before.activeLeases && after.initializationCount == before.initializationCount && after.shutdownCount == before.shutdownCount, "validation failure does not acquire or cycle the process runtime");
    }

    void TestRuntimeOwnershipCapabilitiesAndHandleDomains()
    {
        const Physics::PhysicsRuntime::Statistics baseline = Physics::PhysicsRuntime::GetStatistics();
        Check(baseline.activeLeases == 0 && !baseline.initialized, "physics contract test starts without an active runtime lease");

        Physics::PhysicsSystemDesc desc;
        desc.initDesc.workerThreadCount = 1;

        {
            Physics::PhysicsScene sceneA(desc);
            Check(sceneA.IsInitialized(), "first PhysicsScene initializes");
            if (!sceneA.IsInitialized())
                return;

            const Physics::PhysicsRuntime::Statistics afterFirst = Physics::PhysicsRuntime::GetStatistics();
            Check(afterFirst.activeLeases == baseline.activeLeases + 1, "first Scene owns one runtime lease");
            Check(afterFirst.initializationCount == baseline.initializationCount + 1 && afterFirst.initialized, "first lease initializes the process runtime exactly once");

            Physics::PhysicsScene sceneB(desc);
            Check(sceneB.IsInitialized(), "second PhysicsScene initializes while the first is alive");
            if (!sceneB.IsInitialized())
                return;

            const Physics::PhysicsRuntime::Statistics afterSecond = Physics::PhysicsRuntime::GetStatistics();
            Check(afterSecond.activeLeases == baseline.activeLeases + 2, "two Scenes hold two runtime leases");
            Check(afterSecond.initializationCount == afterFirst.initializationCount, "second Scene reuses process registration instead of registering Jolt again");

            const Physics::PhysicsResult repeatedInitialize = sceneA.Initialize(desc);
            Check(repeatedInitialize.status == Physics::PhysicsStatus::AlreadyInitialized, "repeated Scene initialization has an explicit AlreadyInitialized result");
            Check(Physics::PhysicsRuntime::GetStatistics().activeLeases == afterSecond.activeLeases, "repeated Scene initialization does not leak a runtime lease");

            const Physics::PhysicsBackendCapabilities capabilities = sceneA.GetCapabilities();
            Check(capabilities.boxShape && capabilities.sphereShape && capabilities.closestRaycast, "Jolt capabilities advertise the implemented Step 0.1 features");
            Check(!capabilities.capsuleShape && !capabilities.constraints && !capabilities.continuousCollisionDetection, "unimplemented Jolt features are explicit capabilities");
            Check(capabilities.SupportsShape(Physics::ColliderShapeType::Box) && capabilities.SupportsShape(Physics::ColliderShapeType::Sphere) && !capabilities.SupportsShape(Physics::ColliderShapeType::Capsule), "shape capability lookup matches individual flags");

            Physics::PhysicsBodyCreateDesc capsuleDesc = MakeStaticBox(100);
            capsuleDesc.shapeDesc.type = Physics::ColliderShapeType::Capsule;
            const Physics::PhysicsBodyCreateResult capsule = sceneA.CreateBodyImmediate(capsuleDesc);
            Check(capsule.result.status == Physics::PhysicsStatus::UnsupportedFeature && !capsule.handle, "unsupported Capsule creation returns UnsupportedFeature without allocating a handle");

            Physics::PhysicsBodyCreateDesc invalidBoxDesc = MakeStaticBox(100);
            invalidBoxDesc.shapeDesc.halfExtents.x = 0.0f;
            const Physics::PhysicsBodyCreateResult invalidBox = sceneA.CreateBodyImmediate(invalidBoxDesc);
            Check(invalidBox.result.status == Physics::PhysicsStatus::InvalidArgument && !invalidBox.handle, "invalid collider dimensions are rejected before backend body allocation");

            Physics::PhysicsBodyCreateDesc invalidRotationDesc = MakeStaticBox(100);
            invalidRotationDesc.rotation = { 0, 0, 0, 0 };
            const Physics::PhysicsBodyCreateResult invalidRotation = sceneA.CreateBodyImmediate(invalidRotationDesc);
            Check(invalidRotation.result.status == Physics::PhysicsStatus::InvalidArgument && !invalidRotation.handle, "non-normalized body rotations are rejected with InvalidArgument");

            const Physics::PhysicsBodyCreateResult originalA = sceneA.CreateBodyImmediate(MakeStaticBox(101));
            const Physics::PhysicsBodyCreateResult bodyB = sceneB.CreateBodyImmediate(MakeStaticBox(201));
            Check(originalA.Succeeded() && bodyB.Succeeded(), "independent Scenes create backend-owned bodies");
            if (!originalA || !bodyB)
                return;

            Check(!sceneA.HasBody(bodyB.handle) && !sceneB.HasBody(originalA.handle), "body handles are rejected by the wrong Scene");
            Check(!sceneA.SetBodyTransform(bodyB.handle, {}, { 0, 0, 0, 1 }), "wrong-Scene body mutations are rejected");

            Check(sceneA.EnqueueRigidbodyDestroy(originalA.handle), "live body can be queued for destruction");
            sceneA.Tick(1.0f / 60.0f);
            Check(!sceneA.HasBody(originalA.handle), "destroyed body handle becomes stale");
            Check(!sceneA.EnqueueRigidbodyDestroy(originalA.handle) && !sceneA.SetBodyTransform(originalA.handle, {}, { 0, 0, 0, 1 }), "stale handle is rejected by lookup and mutation APIs");

            const Physics::PhysicsBodyCreateResult replacementA = sceneA.CreateBodyImmediate(MakeStaticBox(102));
            Check(replacementA.Succeeded(), "destroyed registry slot can be reused");
            if (!replacementA)
                return;
            Check(replacementA.handle.Index() == originalA.handle.Index(), "registry reuses the released body index");
            Check(replacementA.handle.Generation() != originalA.handle.Generation(), "reused body index receives a new generation");
            Check(replacementA.handle != bodyB.handle, "generation reuse cannot collide with a live handle in another Scene");
            Check(!sceneA.HasBody(bodyB.handle) && !sceneB.HasBody(replacementA.handle), "wrong-Scene rejection remains valid after generation reuse");

            sceneA.Shutdown();
            const Physics::PhysicsRuntime::Statistics afterFirstShutdown = Physics::PhysicsRuntime::GetStatistics();
            Check(afterFirstShutdown.activeLeases == baseline.activeLeases + 1 && afterFirstShutdown.initialized, "destroying one Scene retains the runtime for the remaining Scene");
            Check(afterFirstShutdown.shutdownCount == baseline.shutdownCount, "runtime is not unregistered while another Scene lease exists");
            Check(sceneB.HasBody(bodyB.handle), "remaining Scene stays usable after the other Scene shuts down");

            Physics::RaycastHit hit;
            Check(sceneB.Raycast({ 0, 5, 0 }, { 0, -1, 0 }, 10.0f, hit), "remaining Scene can query its world after the other Scene shuts down");
            Check(hit.hasHit && hit.bodyHandle == bodyB.handle && hit.gameObjectId == 201, "raycast returns engine handle and owner id without exposing Jolt BodyID");

            sceneA.Shutdown();
            Check(Physics::PhysicsRuntime::GetStatistics().activeLeases == afterFirstShutdown.activeLeases, "repeated Shutdown does not release another Scene's lease");

            sceneB.Shutdown();
            const Physics::PhysicsRuntime::Statistics afterFinalShutdown = Physics::PhysicsRuntime::GetStatistics();
            Check(afterFinalShutdown.activeLeases == baseline.activeLeases && !afterFinalShutdown.initialized, "final Scene shutdown releases the final runtime lease");
            Check(afterFinalShutdown.shutdownCount == baseline.shutdownCount + 1, "final lease unregisters the process runtime exactly once");
            sceneB.Tick(1.0f / 60.0f);
            Check(!sceneB.HasBody(bodyB.handle), "Scene APIs remain safe after shutdown");
        }

        const Physics::PhysicsRuntime::Statistics afterConcurrentScenes = Physics::PhysicsRuntime::GetStatistics();
        Check(afterConcurrentScenes.activeLeases == baseline.activeLeases && afterConcurrentScenes.initializationCount == baseline.initializationCount + 1 && afterConcurrentScenes.shutdownCount == baseline.shutdownCount + 1 && !afterConcurrentScenes.initialized, "Scene destructors do not double-release an explicitly closed runtime");

        {
            Physics::PhysicsScene sequentialScene(desc);
            Check(sequentialScene.IsInitialized(), "runtime can initialize a new Scene after the previous final lease shuts down");
            sequentialScene.Shutdown();
        }

        const Physics::PhysicsRuntime::Statistics final = Physics::PhysicsRuntime::GetStatistics();
        Check(final.activeLeases == baseline.activeLeases && final.initializationCount == baseline.initializationCount + 2 && final.shutdownCount == baseline.shutdownCount + 2 && !final.initialized, "sequential Scene lifetime re-registers and unregisters the process runtime exactly once");
    }

    void TestPublicHeadersDoNotLeakJolt()
    {
        const std::filesystem::path publicInclude = std::filesystem::path(CHIKA_PROJECT_ROOT_DIR) / "engine" / "Runtime" / "Physics" / "include";
        Check(std::filesystem::is_directory(publicInclude), "physics public include directory exists");
        if (!std::filesystem::is_directory(publicInclude))
            return;

        constexpr std::string_view forbiddenTokens[]{ "Jolt/", "JPH::", "namespace JPH", "PhysicsJoltBackend", "JoltLayer" };
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(publicInclude))
        {
            if (!entry.is_regular_file())
                continue;
            const std::string extension = entry.path().extension().string();
            if (extension != ".h" && extension != ".hpp")
                continue;

            std::ifstream input(entry.path(), std::ios::binary);
            const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            for (const std::string_view forbidden : forbiddenTokens)
            {
                if (contents.find(forbidden) == std::string::npos)
                    continue;
                const std::string message = "public physics header leaks private Jolt token '" + std::string(forbidden) + "': " + entry.path().string();
                Check(false, message.c_str());
            }
        }
    }
} // namespace

int main()
{
    TestStrongHandleContract();
    TestDefaultContract();
    TestFailedInitializationIsSafe();
    TestRuntimeOwnershipCapabilitiesAndHandleDomains();
    TestPublicHeadersDoNotLeakJolt();

    if (g_failures == 0)
        std::cout << "Physics contract checks passed\n";
    else
        std::cerr << g_failures << " physics contract test(s) failed\n";
    return g_failures == 0 ? 0 : 1;
}
