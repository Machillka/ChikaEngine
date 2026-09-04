#include "ChikaEngine/component/ScriptComponent.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"

namespace py = pybind11;

namespace ChikaEngine::Framework
{
    namespace
    {
        const char* ToString(Physics::PhysicsPairPhase phase) noexcept
        {
            switch (phase)
            {
            case Physics::PhysicsPairPhase::Enter:
                return "enter";
            case Physics::PhysicsPairPhase::Stay:
                return "stay";
            case Physics::PhysicsPairPhase::Exit:
                return "exit";
            }
            return "unknown";
        }

        const char* ToString(Physics::PhysicsPairKind kind) noexcept
        {
            return kind == Physics::PhysicsPairKind::Trigger ? "trigger" : "collision";
        }

        const char* ToString(Physics::PhysicsPairTerminationReason reason) noexcept
        {
            switch (reason)
            {
            case Physics::PhysicsPairTerminationReason::None:
                return "none";
            case Physics::PhysicsPairTerminationReason::Separated:
                return "separated";
            case Physics::PhysicsPairTerminationReason::BodyDestroyed:
                return "body_destroyed";
            case Physics::PhysicsPairTerminationReason::FilterChanged:
                return "filter_changed";
            }
            return "unknown";
        }

        py::object BuildPhysicsPayload(const PhysicsContactEvent& event)
        {
            py::dict payload;
            payload["phase"] = ToString(event.phase);
            payload["kind"] = ToString(event.kind);
            payload["self_game_object_id"] = py::int_(event.selfGameObject);
            payload["other_game_object_id"] = py::int_(event.otherGameObject);
            payload["self_body_handle"] = py::int_(event.selfBody.Value());
            payload["other_body_handle"] = py::int_(event.otherBody.Value());
            payload["self_collider_handle"] = py::int_(event.selfCollider.Value());
            payload["other_collider_handle"] = py::int_(event.otherCollider.Value());
            payload["point"] = py::make_tuple(event.contact.point.x, event.contact.point.y, event.contact.point.z);
            payload["normal"] = py::make_tuple(event.contact.normal.x, event.contact.normal.y, event.contact.normal.z);
            payload["relative_velocity"] = py::make_tuple(event.contact.relativeVelocity.x, event.contact.relativeVelocity.y, event.contact.relativeVelocity.z);
            payload["penetration"] = event.contact.penetration;
            payload["impulse"] = event.contact.impulse;
            payload["has_contact_data"] = event.hasContactData;
            payload["has_point"] = event.contact.hasPoint;
            payload["has_normal"] = event.contact.hasNormal;
            payload["has_penetration"] = event.contact.hasPenetration;
            payload["has_relative_velocity"] = event.contact.hasRelativeVelocity;
            payload["has_impulse"] = event.contact.hasImpulse;
            payload["termination_reason"] = ToString(event.terminationReason);
            payload["self_alive"] = event.selfAlive;
            payload["other_alive"] = event.otherAlive;
            payload["fixed_step_index"] = py::int_(event.fixedStepIndex);
            return py::module_::import("types").attr("MappingProxyType")(payload);
        }
    } // namespace

    ScriptComponent::~ScriptComponent()
    {
        _pythonInstance = py::none();
    }

    void ScriptComponent::Awake()
    {
        if ((scriptAsset.IsValid() || !scriptAsset.diagnosticPath.empty()) && GetOwner() && GetOwner()->GetScene() && GetOwner()->GetScene()->GetAssetManager())
        {
            const Asset::AssetRecord* scriptRecord = GetOwner()->GetScene()->GetAssetManager()->ResolveReference(scriptAsset, Asset::AssetType::Script, GetOwner()->GetName() + ".ScriptComponent");
            if (!scriptRecord)
                return;
            moduleName = scriptRecord->sourcePath.stem().string();
        }
        if (moduleName.empty() || className.empty())
            return;
        try
        {
            py::module_ module = py::module_::import(moduleName.c_str());
            py::object pythonClass = module.attr(className.c_str());
            _pythonInstance = pythonClass();
            _isLoaded = true;
            _pythonInstance.attr("owner") = py::cast(GetOwner());
            Invoke("awake");
        }
        catch (py::error_already_set& error)
        {
            LOG_ERROR("Python", "Failed to load script {}.{}: {}", moduleName, className, error.what());
            _isLoaded = false;
        }
    }

    void ScriptComponent::Invoke(const char* functionName)
    {
        if (!_isLoaded || !py::hasattr(_pythonInstance, functionName))
            return;
        try
        {
            _pythonInstance.attr(functionName)();
        }
        catch (py::error_already_set& error)
        {
            LOG_ERROR("Python", "{}: {}", functionName, error.what());
        }
    }

    void ScriptComponent::Invoke(const char* functionName, float value)
    {
        if (!_isLoaded || !py::hasattr(_pythonInstance, functionName))
            return;
        try
        {
            _pythonInstance.attr(functionName)(value);
        }
        catch (py::error_already_set& error)
        {
            LOG_ERROR("Python", "{}: {}", functionName, error.what());
        }
    }

    void ScriptComponent::InvokePhysics(const char* functionName, const PhysicsContactEvent& event)
    {
        if (!_isLoaded || !py::hasattr(_pythonInstance, functionName))
            return;
        try
        {
            _pythonInstance.attr(functionName)(BuildPhysicsPayload(event));
        }
        catch (py::error_already_set& error)
        {
            LOG_ERROR("Python", "{}: {}", functionName, error.what());
        }
    }

    void ScriptComponent::Start()
    {
        Invoke("start");
    }

    void ScriptComponent::FixedTick(float fixedDeltaTime)
    {
        Invoke("fixed_update", fixedDeltaTime);
    }

    void ScriptComponent::Tick(float deltaTime)
    {
        Invoke("update", deltaTime);
    }

    void ScriptComponent::LateTick(float deltaTime)
    {
        Invoke("late_update", deltaTime);
    }

    void ScriptComponent::OnEnable()
    {
        Invoke("on_enable");
    }

    void ScriptComponent::OnDisable()
    {
        Invoke("on_disable");
    }

    void ScriptComponent::OnDestroy()
    {
        Invoke("on_destroy");
        _isLoaded = false;
        _pythonInstance = py::none();
    }

    void ScriptComponent::OnCollisionEnter(const PhysicsContactEvent& event)
    {
        InvokePhysics("on_collision_enter", event);
    }

    void ScriptComponent::OnCollisionStay(const PhysicsContactEvent& event)
    {
        InvokePhysics("on_collision_stay", event);
    }

    void ScriptComponent::OnCollisionExit(const PhysicsContactEvent& event)
    {
        InvokePhysics("on_collision_exit", event);
    }

    void ScriptComponent::OnTriggerEnter(const PhysicsContactEvent& event)
    {
        InvokePhysics("on_trigger_enter", event);
    }

    void ScriptComponent::OnTriggerStay(const PhysicsContactEvent& event)
    {
        InvokePhysics("on_trigger_stay", event);
    }

    void ScriptComponent::OnTriggerExit(const PhysicsContactEvent& event)
    {
        InvokePhysics("on_trigger_exit", event);
    }
} // namespace ChikaEngine::Framework
