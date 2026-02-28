
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.h"

namespace ChikaEngine::Framework
{
    Renderable::Renderable() : Component(), _matHandle(0), _meshHandle(0), _visible(true), _isRegisterToScene(false) {}
    Renderable::~Renderable()
    {
        UnregisterFromScene();
    }

    void Renderable::OnEnable()
    {
        Component::OnEnable();
        RegisterToScene();
    }
    void Renderable::OnDisable()
    {
        Component::OnDisable();
        UnregisterFromScene();
    }
    void Renderable::OnDestroy()
    {
        UnregisterFromScene();
    }

    ChikaEngine::Render::RenderObject Renderable::BuildRenderObject() const
    {
        Render::RenderObject obj{.material = _matHandle, .mesh = _meshHandle};

        if (GetOwner() && GetOwner()->transform)
        {
            obj.modelMat = GetOwner()->transform->GetLocalMatrix();
        }
        else
        {
            obj.modelMat = Math::Mat4::Identity();
        }

        return obj;
    }

    void Renderable::RegisterToScene()
    {
        if (_isRegisterToScene)
            return;
        if (!GetOwner() || !GetOwner()->GetScene())
            return;
        GetOwner()->GetScene()->RegisterRenderable(this);
        _isRegisterToScene = true;
    }
    void Renderable::UnregisterFromScene()
    {
        if (!_isRegisterToScene)
            return;
        if (!GetOwner() || !GetOwner()->GetScene())
            return;
        GetOwner()->GetScene()->UnregisterRenderable(this);
        _isRegisterToScene = false;
    }
} // namespace ChikaEngine::Framework