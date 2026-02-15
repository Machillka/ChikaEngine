#pragma once

#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/renderobject.h"
#include "ChikaEngine/serialization/Access.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ChikaEngine::Framework
{
    // 可被管理的 Scene 实例
    // TODO[x]: 实现 Scene Manager 管理多个 Scene 支持切换加载等
    // TODO: 序列化实现 json 读写关卡
    /*!
        不论是 play mode 还是 edit mode
        处理的 GO 是完全一致的,包括是否要 进行渲染
        基于此, 不需要改动 go 相关逻辑,只需要记录 edit 时候 go 的各个状态;
        然后在 play mode 启动的时候执行一个快照 ( 序列化 )
        最后在 play mode 销毁的时候, 恢复快照 ( 反序列化 )
        因为 每个 Scene 都有自己的 play 模式和 edit 模式,所以不在此处使用单例
        最后在上层循环中,根据当前状态来切换 update 的执行即可
        go 的 update 逻辑不变化,
        只是在 play mode 的时候 加入物理计算和更新位置信息
    */
    class Scene
    {
      public:
        Scene();
        ~Scene();

        void InitScene();

        // gameobejct 管理
        GameObject* CreateGameObject(const std::string& name = "");
        void DestroyGameObject(Core::GameObjectID id);
        void DestoryGO(GameObject* go);

        GameObject* GetGameObjectByID(Core::GameObjectID id);
        std::vector<GameObject> GetAllGameObjects();
        std::vector<Render::RenderObject> GetAllVisiableRenderObjects();

        // 编辑器更新：只处理编辑器摄像机、Gizmos，不跑物理
        // TODO: 提供 resize 方法
        // void OnViewportResize(uint32_t width, uint32_t height);

        void RegisterRenderable(Renderable* comp);
        void UnregisterRenderable(Renderable* comp);

        template <typename Archive> void Serialize(Archive& ar)
        {
            using namespace ChikaEngine::Serialization;

            size_t count = _objects.size();
            ar("ObjectCount", count);

            // 在编译器自动生成存储和拉取的代码
            if constexpr (Archive::IsSaving)
            {
                for (auto& kv : _objects)
                {
                    // 序列化每一个 GameObject 实例
                    ar(*kv.second);
                }
            }
            else
            {
                ClearAllObjects();
                for (size_t i = 0; i < count; ++i)
                {
                    auto go = CreateGameObject();
                    ar(*go); // 反序列化填充 GO 数据 (包括 ID)
                    // TODO: 完善更新逻辑
                }
            }
        }

      public:
        // 声明周期函数
        void OnStart();
        void OnUpdate(float deltaTime);
        void OnFixedUpdate(float fixedDeltaTime);
        void OnPhysicsUpdate(float fixedDeltaTime);
        void OnEnd(); // 序列化保存

      public:
        // 系统相关方法
        void RegisterPhysicsScene(Physics::PhysicsScene* physicsScene); // 注册,如果有的话就在update中update,如果没有就没有
        Physics::PhysicsScene* GetPhysicsScene();

        // TODO: 目前 render 依旧全局单例, 先不动 ( 因为有单独渲染到 target 的能力 )

        // 输出 scene 所有需要关注的内容
        std::vector<uint8_t> OutputSceneBytes();

      private:
        void ClearAllObjects();

      private:
        // 缓存并且管理 scene 下的所有 GO
        std::unordered_map<Core::GameObjectID, std::unique_ptr<GameObject>> _objects;

        std::mutex _renderMutex;
        std::vector<Renderable*> _renderables;
        bool _isRunning = false;

        std::unique_ptr<Physics::PhysicsScene> _physicsScene = nullptr;
        std::vector<uint8_t> _snapshotBuffer; // 缓存
    };
} // namespace ChikaEngine::Framework