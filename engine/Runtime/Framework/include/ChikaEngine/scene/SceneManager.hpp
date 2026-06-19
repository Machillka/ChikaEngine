#pragma once

#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/scene/scene.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace ChikaEngine::Framework
{
    class SceneManager
    {
      public:
        bool Initialize(const SceneCreateInfo& createInfo);
        void Shutdown();

        Scene* CreateScene(const std::string& name, bool makeActive = false);
        bool DestroyScene(const std::string& name);
        bool SetActiveScene(const std::string& name);
        bool SaveScene(const std::string& name, const std::string& path) const;
        Scene* LoadScene(const std::string& name, const std::string& path, bool makeActive = false);
        /** @brief 按 AssetDatabase 中的稳定 Scene GUID 加载并可选激活 Scene。 */
        Scene* LoadScene(const Asset::AssetGuid& guid, bool makeActive = false);

        Scene* GetActiveScene() const
        {
            return _activeScene;
        }
        Scene* GetScene(const std::string& name) const;
        const std::string& GetActiveSceneName() const
        {
            return _activeSceneName;
        }

        void Tick(float deltaTime);

      private:
        SceneCreateInfo _createInfo;
        std::unordered_map<std::string, std::unique_ptr<Scene>> _scenes;
        Scene* _activeScene = nullptr;
        std::string _activeSceneName;
        bool _initialized = false;
    };
} // namespace ChikaEngine::Framework
