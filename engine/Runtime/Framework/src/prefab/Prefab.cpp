#include "ChikaEngine/prefab/Prefab.hpp"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/io/IStream.h"
#include "ChikaEngine/io/MemoryStream.h"
#include "ChikaEngine/scene/SceneEvents.hpp"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/serialization/JsonLoadArchive.h"
#include "ChikaEngine/serialization/JsonSaveArchive.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ChikaEngine::Framework
{
    bool Prefab::Capture(const Scene& scene, Core::GameObjectID rootId)
    {
        auto* root = const_cast<Scene&>(scene).GetGameObject(rootId);
        if (!root)
            return false;

        std::vector<GameObject*> objects;
        std::function<void(GameObject*)> collect = [&](GameObject* object)
        {
            if (!object)
                return;
            objects.push_back(object);
            if (!object->transform)
                return;
            for (auto* child : object->transform->GetChildren())
            {
                if (child)
                    collect(child->GetOwner());
            }
        };
        collect(root);

        IO::MemoryStream stream;
        {
            Serialization::JsonSaveArchive archive(stream);
            archive.EnterNode("Prefab");
            archive("RootId", rootId);
            std::size_t count = objects.size();
            archive.EnterArray("Objects", count);
            for (auto* object : objects)
            {
                archive.EnterNode(nullptr);
                archive("GameObject", *object);
                archive.LeaveNode();
            }
            archive.LeaveArray();
            archive.LeaveNode();
        }

        _data = stream.GetRawData();
        _rootSourceId = rootId;
        return true;
    }

    Core::GameObjectID Prefab::Instantiate(Scene& scene, const std::string& rootName) const
    {
        if (!IsValid())
            return Core::InvalidGameObjectID;

        IO::MemoryStream stream(_data.data(), _data.size());
        Serialization::JsonLoadArchive archive(stream);
        archive.EnterNode("Prefab");

        Core::GameObjectID sourceRootId = Core::InvalidGameObjectID;
        archive("RootId", sourceRootId);

        std::size_t count = 0;
        archive.EnterArray("Objects", count);

        std::vector<std::unique_ptr<GameObject>> objects;
        objects.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            archive.EnterNode(nullptr);
            auto object = std::make_unique<GameObject>();
            object->SetScene(&scene);
            archive("GameObject", *object);
            objects.emplace_back(std::move(object));
            archive.LeaveNode();
        }
        archive.LeaveArray();
        archive.LeaveNode();

        std::unordered_map<Core::GameObjectID, Core::GameObjectID> idRemap;
        for (auto& object : objects)
        {
            const auto sourceId = object->GetID();
            const auto instanceId = Core::UIDGenerator::Instance().Generate();
            idRemap[sourceId] = instanceId;
            object->SetID(instanceId);
        }

        for (auto& object : objects)
        {
            if (!object->transform)
                object->transform = object->GetComponent<Transform>();
            if (!object->transform)
                continue;

            const auto parentIt = idRemap.find(object->transform->GetParentId());
            object->transform->SetSerializedParentId(parentIt == idRemap.end() ? Core::InvalidGameObjectID : parentIt->second);
        }

        const auto rootIt = idRemap.find(sourceRootId);
        if (rootIt == idRemap.end())
            return Core::InvalidGameObjectID;
        const auto instanceRootId = rootIt->second;

        std::vector<GameObject*> added;
        added.reserve(objects.size());
        for (auto& object : objects)
        {
            auto* raw = object.get();
            scene._gameobjectMap[raw->GetID()] = raw;
            scene._gameobjects.emplace_back(std::move(object));
            added.push_back(raw);
        }

        for (auto* object : added)
            object->OnDeserialized();
        for (auto* object : added)
        {
            if (object->transform)
                object->transform->ResetHierarchyLinks();
        }
        for (auto* object : added)
        {
            if (object->transform)
                object->transform->ResolveHierarchy(scene);
        }
        for (auto* object : added)
        {
            object->RefreshActiveInHierarchy();
            if (scene.IsPlaying())
                object->BeginPlay();
            scene._eventBus.Publish(GameObjectCreatedEvent{ .scene = &scene, .gameObjectId = object->GetID() });
        }

        if (!rootName.empty())
        {
            if (auto* rootObject = scene.GetGameObject(instanceRootId))
                rootObject->SetName(rootName);
        }
        return instanceRootId;
    }

    void Prefab::SaveToStream(IO::IStream& stream) const
    {
        if (!_data.empty())
            stream.Write(_data.data(), _data.size());
    }

    bool Prefab::LoadFromStream(IO::IStream& stream)
    {
        if (!stream.IsReadingMode() || stream.GetLength() == 0)
            return false;

        _data.resize(stream.GetLength());
        stream.Read(_data.data(), _data.size());

        IO::MemoryStream memory(_data.data(), _data.size());
        Serialization::JsonLoadArchive archive(memory);
        archive.EnterNode("Prefab");
        archive("RootId", _rootSourceId);
        archive.LeaveNode();
        return Core::IsValidGameObjectID(_rootSourceId);
    }
} // namespace ChikaEngine::Framework
