#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/gameobject/GameObject.h"

namespace ChikaEngine::Framework
{
    void Component::SetEnabled(bool enabled)
    {
        if (_isEnabled == enabled || _isDestroyed)
            return;

        _isEnabled = enabled;
        if (_owner)
            _owner->RefreshComponentActivation(*this);
    }
} // namespace ChikaEngine::Framework
