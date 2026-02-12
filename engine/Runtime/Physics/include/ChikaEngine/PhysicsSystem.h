// 把旧的物理系统下沉 可以做成拥有多个物理后端
#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsScene.h"
#include <memory>
namespace ChikaEngine::Physics
{
    class PhysicsSystem
    {
      public:
        static PhysicsSystem& Instance();
        // 工厂方法
        std::unique_ptr<PhysicsScene> CreatePhysicsScene(PhysicsSystemDesc& desc);
        // 通过 handle 访问后端
        // TODO: 应该是重新对 physics scene 暴露接口 还是让 scene 自己持有后端 ??

      private:
        PhysicsSystem();
        ~PhysicsSystem();
    };
} // namespace ChikaEngine::Physics