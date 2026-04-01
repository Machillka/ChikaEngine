#pragma once

#include "ChikaEngine/component/Component.h"
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
        void Update(float deltaTime) override;
        // void FixedUpdate(float fixedDeltaTime) override;
        // void OnDestroy() override;

      private:
        pybind11::object _pythonInstance;
        bool _isLoaded = false;
    };
} // namespace ChikaEngine::Framework