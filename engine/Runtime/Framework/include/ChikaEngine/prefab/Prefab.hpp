#pragma once

#include "ChikaEngine/base/UIDGenerator.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ChikaEngine::Framework
{
    class GameObject;
    class Scene;
} // namespace ChikaEngine::Framework

namespace ChikaEngine::IO
{
    class IStream;
}

namespace ChikaEngine::Framework
{
    class Prefab
    {
      public:
        bool Capture(const Scene& scene, Core::GameObjectID rootId);
        Core::GameObjectID Instantiate(Scene& scene, const std::string& rootName = {}) const;

        void SaveToStream(IO::IStream& stream) const;
        bool LoadFromStream(IO::IStream& stream);

        bool IsValid() const
        {
            return !_data.empty() && Core::IsValidGameObjectID(_rootSourceId);
        }

      private:
        std::vector<std::uint8_t> _data;
        Core::GameObjectID _rootSourceId = Core::InvalidGameObjectID;
    };
} // namespace ChikaEngine::Framework
