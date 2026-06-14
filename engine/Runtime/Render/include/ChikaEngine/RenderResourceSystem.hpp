#pragma once

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include <memory>

namespace ChikaEngine::Render
{
    /**
     * @brief 管理 Asset 到 GPU Resource 的适配器生命周期。
     *
     * 当前复用 ResourceManager 实现；该边界为后续上传调度和资源回收独立演进提供入口。
     */
    class RenderResourceSystem
    {
      public:
        /** @brief 建立 Asset 到 GPU Resource 的适配器，并显式注入依赖。 */
        void Initialize(IRHIDevice& rhi, Asset::AssetManager& assetManager);
        /** @brief 在 RHI 销毁前释放 ResourceManager 管理的 GPU 资源。 */
        void Shutdown();

        Resource::ResourceManager* GetResourceManager() const
        {
            return m_resourceManager.get();
        }

      private:
        std::unique_ptr<Resource::ResourceManager> m_resourceManager;
    };
} // namespace ChikaEngine::Render
