#pragma once

#include "ChikaEngine/AssetLayouts.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace ChikaEngine::Asset
{
    inline constexpr uint32_t MAX_ENVIRONMENT_CUBEMAP_FACE_SIZE = 16'384;

    enum class EnvironmentProjectionStatus : uint8_t
    {
        Success,
        InvalidSource,
        InvalidAspectRatio,
        InvalidFaceSize,
        InvalidFloatPayload,
    };

    struct EnvironmentProjectionOptions
    {
        /** @brief 为 0 时使用 sourceWidth / 4。 */
        uint32_t outputFaceSize = 0;
        uint32_t maxFaceSize = MAX_ENVIRONMENT_CUBEMAP_FACE_SIZE;
    };

    struct EnvironmentProjectionResult
    {
        std::unique_ptr<TextureData> texture;
        EnvironmentProjectionStatus status = EnvironmentProjectionStatus::InvalidSource;
        std::string message;
        double conversionMilliseconds = 0.0;

        explicit operator bool() const
        {
            return texture != nullptr && status == EnvironmentProjectionStatus::Success;
        }
    };

    /**
     * @brief 将线性浮点 2:1 equirectangular 图像转换为 px,nx,py,ny,pz,nz Cubemap。
     *
     * 转换只处理 mip 0，U 方向 wrap、V 方向 clamp，并使用双线性过滤。输出保持输入的
     * Float16/Float32 storage，不执行 tone mapping、gamma 或 clamp-to-1。
     */
    EnvironmentProjectionResult ConvertEquirectangularToCubemap(const TextureData& source, const EnvironmentProjectionOptions& options = {});

    /** @brief 返回 Vulkan/Cubemap 约定下指定 face texel 对应的归一化世界方向。 */
    std::array<float, 3> CubemapTexelDirection(uint32_t face, float u, float v);
} // namespace ChikaEngine::Asset
