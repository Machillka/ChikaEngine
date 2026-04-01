#pragma once
#include "ChikaEngine/debug/log_macros.h"
#include "ResourceTypes.h"
#include "stb_image.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <array>

namespace ChikaEngine::Resource::Importer
{
    struct TextureCubeData
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        bool sRGB = true;
        // 存储 6 个面的像素数据
        std::array<std::vector<unsigned char>, 6> facesPixels;
    };

    struct TextureCubeImporter
    {
        static TextureCubeData Load(const TextureCubeSourceDesc& desc)
        {
            TextureCubeData cubeData;
            cubeData.sRGB = desc.sRGB;

            for (int i = 0; i < 6; ++i)
            {
                int w, h, ch;
                unsigned char* data = stbi_load(desc.facePaths[i].c_str(), &w, &h, &ch, 0);

                if (!data)
                {
                    LOG_ERROR("Texture Importer", "Cubemap face load failed: {}", desc.facePaths[i]);
                    throw std::runtime_error("Cubemap face load failed: " + desc.facePaths[i]);
                }

                // 第一张图决定整个 Cubemap 的尺寸
                if (i == 0)
                {
                    cubeData.width = w;
                    cubeData.height = h;
                    cubeData.channels = ch;
                }
                else
                {
                    // 检查后续图片尺寸是否一致
                    if (w != cubeData.width || h != cubeData.height || ch != cubeData.channels)
                    {
                        LOG_ERROR("Texture Importer", "Cubemap face dimension mismatch at index {}: {}", i, desc.facePaths[i]);
                        stbi_image_free(data);
                        throw std::runtime_error("Cubemap dimension mismatch");
                    }
                }

                // 拷贝数据到 vector
                cubeData.facesPixels[i].assign(data, data + w * h * ch);
                stbi_image_free(data);
            }

            LOG_INFO("Texture Importer", "Loaded Cubemap (6 faces), size={}x{}", cubeData.width, cubeData.height);
            return cubeData;
        }
    };
} // namespace ChikaEngine::Resource::Importer