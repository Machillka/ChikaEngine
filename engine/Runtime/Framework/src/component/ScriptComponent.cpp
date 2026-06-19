#include "ChikaEngine/component/ScriptComponent.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"

namespace py = pybind11;

namespace ChikaEngine::Framework
{
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
} // namespace ChikaEngine::Framework
