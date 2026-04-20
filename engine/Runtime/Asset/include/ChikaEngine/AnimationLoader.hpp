#pragma once

#include "AssetAnimation.hpp"
#include <memory>
namespace ChikaEngine::Asset
{

    class AnimationLoader
    {
      public:
        static std::unique_ptr<AnimationClipData> Load(const std::string& path);
    };

} // namespace ChikaEngine::Asset