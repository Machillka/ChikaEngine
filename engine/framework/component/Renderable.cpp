#include "framework/component/Renderable.h"
#include "framework/component/Component.h"
#include "framework/scene/scene.h"
#include "math/mat4.h"
#include "render/renderobject.h"
#include "framework/gameobject/GameObject.h"

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
        Scene::Instance().RegisterRenderable(this);
        _isRegisterToScene = true;
    }
    void Renderable::UnregisterFromScene()
    {
        if (!_isRegisterToScene)
            return;
        Scene::Instance().UnregisterRenderable(this);
        _isRegisterToScene = false;
    }
} // namespace ChikaEngine::Framework