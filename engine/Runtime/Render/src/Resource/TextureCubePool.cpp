#include "ChikaEngine/Resource/TextureCubePool.h"
#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/debug/log_macros.h"
#include <vector>
namespace ChikaEngine::Render
{

    IRHIDevice* TextureCubePool::_device = nullptr;
    std::vector<RHITextureCubeData> TextureCubePool::_textures;

    void TextureCubePool::Init(IRHIDevice* device)
    {
        _device = device;
    }

    void TextureCubePool::Reset()
    {
        for (auto& data : _textures)
        {
            if (data.texture)
            {
                delete data.texture;
                data.texture = nullptr;
            }
        }
        _textures.clear();
        _device = nullptr;
    }

    TextureCubeHandle TextureCubePool::Create(int width, int height, int channels, const std::array<const void*, 6>& facesData, bool sRGB)
    {
        if (!_device)
        {
            LOG_ERROR("TexturePool", "Create called but _device is nullptr! Did you forget to call TextureCubePool::Init?");
            throw std::runtime_error("TexturePool::_device is null");
        }
        IRHITextureCube* tex = _device->CreateTextureCube(width, height, channels, facesData, sRGB);
        RHITextureCubeData data = {.texture = tex};
        _textures.push_back(data);
        TextureCubeHandle handle = static_cast<TextureCubeHandle>(_textures.size() - 1);
        LOG_INFO("TextureCubePool", "Created TextureCube Handle ID={} total={}", handle, _textures.size());

        return handle;
    }

    const RHITextureCubeData& TextureCubePool::Get(TextureCubeHandle handle)
    {
        return _textures[handle];
    }
} // namespace ChikaEngine::Render