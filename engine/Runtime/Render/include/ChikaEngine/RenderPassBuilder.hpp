#pragma once

#include "ChikaEngine/RenderGraphHandle.hpp"
#include "RHIDesc.hpp"
#include <string>
namespace ChikaEngine::Render
{

    class RenderGraph;

    // Builder 声明资源的生命周期和依赖
    class RGPassBuilder
    {
      public:
        RGPassBuilder(class RGPass* pass, RenderGraph* graph) : m_pass(pass), m_graph(graph) {}

        RGTextureHandle CreateTexture(const std::string& name, const TextureDesc& desc);
        void ReadTexture(RGTextureHandle handle, ResourceState state = ResourceState::ShaderResource);
        void WriteColor(RGTextureHandle handle, LoadOp loadOp = LoadOp::Clear, const float clearColor[4] = nullptr);
        void WriteDepth(RGTextureHandle handle, LoadOp loadOp = LoadOp::Clear);

      private:
        class RGPass* m_pass;
        RenderGraph* m_graph;
    };

} // namespace ChikaEngine::Render