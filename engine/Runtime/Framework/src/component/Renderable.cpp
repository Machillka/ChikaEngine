
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.h"

namespace ChikaEngine::Framework
{
    Renderable::Renderable() : Component(), _obj(), _visible(true), _isRegisterToScene(false) {}
    Renderable::~Renderable()
    {
        UnregisterFromScene();
    }

    void Renderable::OnEnable()
    {
        RegisterToScene();
    }
    void Renderable::OnDisable()
    {
        UnregisterFromScene();
    }
    void Renderable::OnDestroy()
    {
        UnregisterFromScene();
    }

    ChikaEngine::Render::RenderObject Renderable::BuildRenderObject() const
    {
        Render::RenderObject obj = _obj;

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
        GetOwner()->GetScene()->RegisterRenderable(this);
        _isRegisterToScene = true;
    }
    void Renderable::UnregisterFromScene()
    {
        if (!_isRegisterToScene)
            return;
        GetOwner()->GetScene()->UnregisterRenderable(this);
        _isRegisterToScene = false;
    }
} // namespace ChikaEngine::Framework