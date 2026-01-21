// render/MeshPool.h
#pragma once
#include "Mesh.h"
#include "render/rhi/RHIDevice.h"
#include "render/rhi/RHIResources.h"
#include "render/rhi/RHITypes.h"

#include <vector>

namespace ChikaEngine::Render
{
    class MeshPool
    {
      public:
        static void Init(IRHIDevice* device);

        static MeshHandle Create(const Mesh& mesh);
        static const RHIMesh& Get(MeshHandle handle);
        static void Reset();

      private:
        static IRHIDevice* _device;
        static std::vector<RHIMesh> _meshes;
    };
} // namespace ChikaEngine::Render
