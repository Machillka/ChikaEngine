/*!
 * @file TexturePool.h
 * @brief 纹理资源池 - 管理 CPU 端纹理元数据和 GPU 端纹理资源
 * @date 2026-03-07
 *
 * 设计理念：
 * - 管理 Texture（CPU 端元数据）和 RHITexture2D（GPU 端资源）的对应关系
 * - 通过 TextureHandle 进行 O(1) 查询
 * - 将数据（Texture）和 GPU 资源（RHITexture2D）分离
 * - 支持多种像素格式的自动选择（R, RG, RGB, RGBA）
 *
 * 架构：
 *   ResourceSystem → 从磁盘加载纹理文件（.png, .jpg 等）
 *            ↓
 *   TexturePool::Create() → 上传像素数据到 GPU，创建纹理对象
 *            ↓
 *   TextureHandle ← 返回版本化句柄
 *            ↓
 *   MaterialPool/RenderDevice → 通过 TexturePool::Get() 查询和绑定纹理
 */
#pragma once

#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/Resource/Texture.h"
#include "ChikaEngine/Resource/ResourceHandles.h"
#include <vector>

namespace ChikaEngine::Render
{
    // RHITexture2D 在 Texture.h 中定义

    /*!
     * @class TexturePool
     * @brief 纹理资源池 - 管理所有纹理的 CPU 和 GPU 资源
     *
     * 职责：
     * - 存储所有 Texture（CPU 端元数据）
     * - 存储所有 RHITexture2D（GPU 端资源）
     * - 提供基于 TextureHandle 的 O(1) 查询
     * - 上传像素数据到 GPU
     * - 自动选择合适的 GPU 格式（R, RG, RGB, RGBA）
     * - 支持 sRGB 色彩空间用于漫反射贴图
     * - 管理纹理对象的生命周期
     *
     * 使用流程：
     * @code
     *   // 1. 初始化池
     *   TexturePool::Init(rhiDevice);
     *
     *   // 2. 创建纹理（通常由 ResourceSystem 调用）
     *   // 方法 A：从原始像素数据创建
     *   TextureHandle handle = TexturePool::Create(
     *       width, height, channels, pixelData, sRGB
     *   );
     *
     *   // 方法 B：从 Texture 对象创建（如果有像素数据）
     *   // TextureHandle handle = TexturePool::Create(textureData);
     *
     *   // 3. 查询和使用
     *   const Texture& cpuData = TexturePool::GetData(handle);    // CPU 端元数据
     *   const RHITexture2D& rhi = TexturePool::GetRHI(handle);   // GPU 端资源
     *
     *   // 4. 绑定到材质
     *   rhiMaterial.textures["diffuse"] = rhi.texture;
     *
     *   // 5. 清理
     *   TexturePool::Reset();
     * @endcode
     *
     * 内存管理：
     * - CPU 端元数据存储在 std::vector<Texture> 中
     * - GPU 端资源（纹理对象、显存）由 RHI 层创建和管理
     * - TexturePool 通过 IRHIDevice::CreateTexture2D() 创建纹理
     * - THandle 包含版本号，防止 use-after-free
     *
     * 格式选择：
     * - channels == 1: 使用 R8 格式（灰度图）
     * - channels == 2: 使用 RG8 格式（法线贴图压缩）
     * - channels == 3: 使用 RGB8 或 SRGB8 格式（漫反射贴图）
     * - channels == 4: 使用 RGBA8 或 SRGBA8 格式（带透明度）
     *
     * 线程安全性：
     * - 当前不是线程安全的
     * - 所有操作应在主线程中执行
     * - GPU 上传应该与渲染线程同步
     */
    class TexturePool
    {
      public:
        /*!
         * @brief 初始化纹理池
         * @param device RHI 设备指针（用于创建纹理对象）
         */
        static void Init(IRHIDevice* device);

        /*!
         * @brief 重置池（清理所有资源）
         */
        static void Reset();

        /*!
         * @brief 从原始像素数据创建纹理
         * @param width 纹理宽度（像素）
         * @param height 纹理高度（像素）
         * @param channels 颜色通道数 (1=R, 2=RG, 3=RGB, 4=RGBA)
         * @param pixels 像素数据指针（RGB/RGBA 字节顺序）
         * @param sRGB 是否使用 sRGB 色彩空间（用于漫反射贴图）
         * @return TextureHandle 版本化纹理句柄
         *
         * 操作流程：
         * 1. 验证输入参数
         * 2. 根据通道数选择合适的 GPU 格式
         * 3. 上传像素数据到 GPU
         * 4. 创建纹理对象并配置采样器
         * 5. 存储 Texture 元数据
         * 6. 返回 TextureHandle
         *
         * @throws std::runtime_error 如果 GPU 上传失败
         */
        static TextureHandle Create(int width, int height, int channels, const unsigned char* pixels, bool sRGB);

        /*!
         * @brief 从 Texture 对象创建纹理
         * @param texture Texture 对象（包含元数据）
         * @param pixelData 像素数据指针
         * @return TextureHandle 版本化纹理句柄
         */
        static TextureHandle Create(const Texture& texture, const unsigned char* pixelData);

        /*!
         * @brief 获取 CPU 端纹理元数据
         * @param handle 纹理句柄
         * @return 纹理元数据（常引用）
         */
        static const Texture& GetData(TextureHandle handle);

        /*!
         * @brief 获取 GPU 端纹理资源
         * @param handle 纹理句柄
         * @return GPU 纹理包装（常引用）
         *
         * 返回值包含：
         * - texture: GPU 纹理对象指针
         * - width, height: 实际纹理尺寸
         * - channels: 颜色通道数
         * - sRGB: 是否使用 sRGB 色彩空间
         */
        static const RHITexture2D& GetRHI(TextureHandle handle);

      private:
        static IRHIDevice* _device;                    ///< RHI 设备指针
        static std::vector<Texture> _textures;         ///< CPU 端纹理元数据
        static std::vector<RHITexture2D> _rhiTextures; ///< GPU 端资源
    };

} // namespace ChikaEngine::Render