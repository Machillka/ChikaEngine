/*!
 * @file rendersubsystem.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-02-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include "ChikaEngine/subsystem/ISubsystem.h"
#include <vector>
namespace ChikaEngine::Framework
{
    class Scene;

    class RenderSubsystem : public ISubsystem
    {
      public:
        RenderSubsystem(Scene* ownerScene, Render::Renderer* renderInstance);
        ~RenderSubsystem() override = default;
        // void Initialize(Scene* ownerWorld) override;
        void Tick(float deltaTime) override;
        void Cleanup() override;

        void CollectDrawCommands();

      private:
        std::vector<Render::DrawCommand> _drawCommands;

        Scene* _ownerScene = nullptr;
        Render::Renderer* _renderer;
        Asset::AssetManager* _assetMgr = nullptr;
        Resource::ResourceManager* _resourceMgr = nullptr;
    };
} // namespace ChikaEngine::Framework