#pragma once

#include "ChikaEngine/base/UIDGenerator.h"

namespace ChikaEngine::Framework
{
    class Component;
    class Scene;

    enum class SceneModes
    {
        Edit,
        EnteringPlay,
        Play,
        Paused,
        ExitingPlay,
    };

    struct SceneModeChangedEvent
    {
        Scene* scene = nullptr;
        SceneModes previousMode = SceneModes::Edit;
        SceneModes currentMode = SceneModes::Edit;
    };

    struct GameObjectCreatedEvent
    {
        Scene* scene = nullptr;
        Core::GameObjectID gameObjectId = Core::InvalidGameObjectID;
    };

    struct GameObjectDestroyedEvent
    {
        Scene* scene = nullptr;
        Core::GameObjectID gameObjectId = Core::InvalidGameObjectID;
    };

    struct ComponentAddedEvent
    {
        Scene* scene = nullptr;
        Core::GameObjectID gameObjectId = Core::InvalidGameObjectID;
        Component* component = nullptr;
    };

    struct ComponentRemovedEvent
    {
        Scene* scene = nullptr;
        Core::GameObjectID gameObjectId = Core::InvalidGameObjectID;
        const char* componentType = nullptr;
    };
} // namespace ChikaEngine::Framework
