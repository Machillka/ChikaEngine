#pragma once

#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/MaterialImporter.h"
#include "ChikaEngine/renderobject.h"

namespace ChikaEngine::Framework
{
    // 前向声明
    class GameObject;
    // 包括 mesh 材质 和 renderobject
    class Renderable : public Component
    {
      public:
        Renderable();
        ~Renderable();
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;

        // 覆盖整个物体
        // TODO: 设计可以针对每个参数进行修改
        void SetMesh(Resource::MeshHandle meshHandle)
        {
            _obj.mesh = meshHandle;
        }
        void SetMaterial(Resource::MaterialHandle materialHandle)
        {
            _obj.material = materialHandle;
        }
        void SetVisible(bool v)
        {
            if (v == _visible)
                return;
            _visible = v;
        }
        bool IsVisible() const
        {
            return _visible;
        }

        // 更新 model 信息
        ChikaEngine::Render::RenderObject BuildRenderObject() const;
        // TODO: 设计图层属性

      private:
        void RegisterToScene();
        void UnregisterFromScene();
        bool _visible;
        Render::RenderObject _obj;
        bool _isRegisterToScene; // 是否已经注册到 Scene 中
    };
} // namespace ChikaEngine::Framework