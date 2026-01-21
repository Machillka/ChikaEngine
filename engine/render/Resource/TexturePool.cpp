#include "TexturePool.h"

#include "debug/log_macros.h"


namespace ChikaEngine::Render
{

    IRHIDevice* TexturePool::_device = nullptr;
    std::vector<RHITexture2D> TexturePool::_textures;
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
    TextureHandle TexturePool::Create(const unsigned char* pixels, int width, int height, bool sRGB)
    {
        // 交给 RHI 创建具体的 GPU 纹理
        IRHITexture2D* tex = _device->CreateTexture2D(width, height, pixels);
        RHITexture2D rhiTex{};
        rhiTex.texture = tex;
        rhiTex.width = width;
        rhiTex.height = height;
        rhiTex.sRGB = sRGB;
        _textures.push_back(rhiTex);
        TextureHandle h = static_cast<TextureHandle>(_textures.size() - 1);
        LOG_INFO("TexturePool", "Created texture handle={} size={}x{} total={}", h, width, height, _textures.size());
        return h;
    }

    const RHITexture2D& TexturePool::Get(TextureHandle handle)
    {
        return _textures[handle];
    }
} // namespace ChikaEngine::Render