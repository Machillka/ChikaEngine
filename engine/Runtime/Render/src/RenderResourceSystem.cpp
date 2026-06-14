#include "ChikaEngine/RenderResourceSystem.hpp"

namespace ChikaEngine::Render
{
    void RenderResourceSystem::Initialize(IRHIDevice& rhi, Asset::AssetManager& assetManager)
    {
        Shutdown();
        m_resourceManager = std::make_unique<Resource::ResourceManager>(rhi, assetManager);
    }

    void RenderResourceSystem::Shutdown()
    {
        m_resourceManager.reset();
    }
} // namespace ChikaEngine::Render
