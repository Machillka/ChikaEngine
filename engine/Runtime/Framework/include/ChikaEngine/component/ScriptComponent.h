#pragma once

#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include <pybind11/pytypes.h>
#include <string>
#include <pybind11/pybind11.h>

namespace ChikaEngine::Framework
{

    MCLASS(ScriptComponent) : public Component
    {
        REFLECTION_BODY(ScriptComponent)

      public:
        ScriptComponent() = default;
        ~ScriptComponent();

        MFIELD()
        std::string moduleName = "player";

        MFIELD()
        std::string className = "PlayerController";

        void Awake() override;
        void Start() override;
        void FixedTick(float fixedDeltaTime) override;
        void Tick(float deltaTime) override;
        void LateTick(float deltaTime) override;
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;

      private:
        void Invoke(const char* functionName);
        void Invoke(const char* functionName, float value);

        pybind11::object _pythonInstance;
        bool _isLoaded = false;
    };
} // namespace ChikaEngine::Framework
