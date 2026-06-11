#include "ChikaEngine/scene/SceneManager.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/io/FileStream.h"
#include <exception>

namespace ChikaEngine::Framework
{
    bool SceneManager::Initialize(const SceneCreateInfo& createInfo)
    {
        Shutdown();
        _createInfo = createInfo;
        _initialized = true;
        return CreateScene("Main", true) != nullptr;
    }

    void SceneManager::Shutdown()
    {
        if (_activeScene && !(_activeScene->IsEditing()))
            _activeScene->StopPlayMode();

        for (auto& [name, scene] : _scenes)
            scene->Shutdown();

        _scenes.clear();
        _activeScene = nullptr;
        _activeSceneName.clear();
        _initialized = false;
    }

    Scene* SceneManager::CreateScene(const std::string& name, bool makeActive)
    {
        if (!_initialized || name.empty() || _scenes.contains(name))
            return nullptr;

        auto scene = std::make_unique<Scene>();
        scene->Initialize(_createInfo);
        auto* result = scene.get();
        _scenes.emplace(name, std::move(scene));

        if (makeActive || !_activeScene)
            SetActiveScene(name);
        return result;
    }

    bool SceneManager::DestroyScene(const std::string& name)
    {
        const auto it = _scenes.find(name);
        if (it == _scenes.end())
            return false;

        if (it->second.get() == _activeScene)
        {
            if (!_activeScene->IsEditing())
                _activeScene->StopPlayMode();
            _activeScene = nullptr;
            _activeSceneName.clear();
        }

        it->second->Shutdown();
        _scenes.erase(it);

        if (!_activeScene && !_scenes.empty())
            SetActiveScene(_scenes.begin()->first);
        return true;
    }

    bool SceneManager::SetActiveScene(const std::string& name)
    {
        const auto it = _scenes.find(name);
        if (it == _scenes.end())
            return false;
        if (_activeScene == it->second.get())
            return true;

        if (_activeScene && !_activeScene->IsEditing())
            _activeScene->StopPlayMode();

        _activeScene = it->second.get();
        _activeSceneName = name;
        return true;
    }

    bool SceneManager::SaveScene(const std::string& name, const std::string& path) const
    {
        auto* scene = GetScene(name);
        if (!scene || !scene->IsEditing())
            return false;

        try
        {
            IO::FileStream stream(path, IO::Mode::Write);
            scene->SaveToStream(stream);
            return true;
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("SceneManager", "Failed to save scene '{}': {}", name, exception.what());
            return false;
        }
    }

    Scene* SceneManager::LoadScene(const std::string& name, const std::string& path, bool makeActive)
    {
        if (!_initialized || name.empty() || _scenes.contains(name))
            return nullptr;

        try
        {
            auto scene = std::make_unique<Scene>();
            scene->Initialize(_createInfo);
            IO::FileStream stream(path, IO::Mode::Read);
            scene->LoadFromStream(stream);

            auto* result = scene.get();
            _scenes.emplace(name, std::move(scene));
            if (makeActive || !_activeScene)
                SetActiveScene(name);
            return result;
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("SceneManager", "Failed to load scene '{}': {}", name, exception.what());
            return nullptr;
        }
    }

    Scene* SceneManager::GetScene(const std::string& name) const
    {
        const auto it = _scenes.find(name);
        return it == _scenes.end() ? nullptr : it->second.get();
    }

    void SceneManager::Tick(float deltaTime)
    {
        if (_activeScene)
            _activeScene->Tick(deltaTime);
    }
} // namespace ChikaEngine::Framework
