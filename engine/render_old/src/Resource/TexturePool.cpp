
#include "ChikaEngine/Resource/TexturePool.h"
#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/debug/log_macros.h"
namespace ChikaEngine::Render
{

    IRHIDevice* TexturePool::_device = nullptr;
    std::vector<Texture> TexturePool::_textures;
    std::vector<RHITexture2D> TexturePool::_rhiTextures;
    void TexturePool::Init(IRHIDevice* device)
    {
        _device = device;
    }
    void TexturePool::Reset()
    {
        _device = nullptr;
        _textures.clear();
    }
    // TODO: 加入 sRGB 的判断
    TextureHandle TexturePool::Create(int width, int height, int channels, const unsigned char* pixels, bool sRGB)
    {
        // 交给 RHI 创建具体的 GPU 纹理
        if (!_device)
        {
            LOG_ERROR("TexturePool", "Create called but _device is nullptr! Did you forget to call TexturePool::Init?");
            throw std::runtime_error("TexturePool::_device is null");
        }
        IRHITexture2D* tex = _device->CreateTexture2D(width, height, channels, pixels, sRGB);
        RHITexture2D rhiTex{};
        rhiTex.texture = tex;

        // store CPU metadata and GPU resource
        Texture texMeta{};
        texMeta.width = width;
        texMeta.height = height;
        texMeta.channels = channels;
        texMeta.srgb = sRGB;
        _textures.push_back(texMeta);
        _rhiTextures.push_back(rhiTex);

        uint32_t index = static_cast<uint32_t>(_textures.size() - 1);
        TextureHandle h = Core::THandle<struct TextureTag>::FromParts(index, 0);
        LOG_INFO("TexturePool", "Created texture index={} size={}x{} total={}", index, width, height, _textures.size());
        return h;
    }

    const Texture& TexturePool::GetData(TextureHandle handle)
    {
        return _textures[handle.GetIndex()];
    }

    const RHITexture2D& TexturePool::GetRHI(TextureHandle handle)
    {
        return _rhiTextures[handle.GetIndex()];
    }
} // namespace ChikaEngine::Render