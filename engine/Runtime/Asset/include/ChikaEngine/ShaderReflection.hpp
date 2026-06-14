#pragma once

#include "ChikaEngine/shader/ShaderInterface.hpp"

#include <filesystem>
#include <span>
#include <string>

namespace ChikaEngine::Asset
{
    /**
     * @brief 提供 Shader Reflection 的导入期提取与持久化入口。
     *
     * Runtime 只加载导入产物，不需要重复调用 SPIRV-Reflect。
     */
    class ShaderReflection
    {
      public:
        /**
         * @brief 从 SPIR-V 字节码提取后端无关 Shader Interface。
         */
        static bool Reflect(std::span<const uint8_t> spirv, Shader::ShaderReflectionData& output, std::string& error);

        /**
         * @brief 保存 Reflection Sidecar；该文件属于源 Shader 的导入产物，不拥有独立 GUID。
         */
        static bool Save(const std::filesystem::path& path, const Shader::ShaderReflectionData& reflection, std::string& error);

        /**
         * @brief 加载已持久化 Reflection Sidecar。
         */
        static bool Load(const std::filesystem::path& path, Shader::ShaderReflectionData& reflection, std::string& error);

        /**
         * @brief 返回与 SPIR-V 产物对应的 Reflection Sidecar 路径。
         */
        static std::filesystem::path SidecarPath(const std::filesystem::path& spirvPath);
    };
} // namespace ChikaEngine::Asset
