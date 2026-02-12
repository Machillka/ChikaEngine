#include "ChikaEngine/InputSystem.h"
#include "ChikaEngine/IInputBackend.h"
#include "ChikaEngine/InputFactory.h"
#include "ChikaEngine/debug/Debug.h"
#include "ChikaEngine/InputDesc.h"

#include <cassert>
#include <memory>
#include <utility>

namespace ChikaEngine::Input
{
    std::unique_ptr<IInputBackend> InputSystem::s_backend = nullptr;
    void InputSystem::Init(InputDesc desc, void* windowHandle)
    {
        InputSystem::Init(InputBackendFactory(desc, windowHandle));
    }
    void InputSystem::Init(std::unique_ptr<IInputBackend> backend)
    {
        s_backend = std::move(backend);
    }

    void InputSystem::Update()
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        s_backend->Update();
    }

    void InputSystem::Shutdown()
    {
        s_backend.reset();
    }

    bool InputSystem::GetKeyUp(KeyCode key)
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        return s_backend->GetKeyUp(key);
    }

    bool InputSystem::GetKeyDown(KeyCode key)
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        return s_backend->GetKeyDown(key);
    }

    bool InputSystem::GetKey(KeyCode key)
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        return s_backend->GetKey(key);
    }

    bool InputSystem::GetMouseButton(MouseButton button)
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        return s_backend->GetMouseButton(button);
    }

    bool InputSystem::GetMouseButtonbDown(MouseButton button)
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        return s_backend->GetMouseButtonDown(button);
    }

    bool InputSystem::GetMouseButtonUp(MouseButton button)
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        return s_backend->GetMouseButtonUp(button);
    }

    std::pair<double, double> InputSystem::GetMousePosition()
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        return s_backend->GetMousePosition();
    }

    std::pair<double, double> InputSystem::GetMouseDelta()
    {
        CHIKA_ASSERT(s_backend, "null input system backend");
        return s_backend->GetMouseDelta();
    }

} // namespace ChikaEngine::Input