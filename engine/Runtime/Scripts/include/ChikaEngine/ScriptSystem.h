/*!
 * @file ScriptSystem.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief  用于启动和关闭脚本系统
 * @version 0.1
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
#include <filesystem>
#include <pybind11/embed.h>
namespace py = pybind11;
namespace ChikaEngine::Scripts
{
    struct ScriptRuntimeConfig
    {
        /** @brief 当前 Project 的绝对脚本目录；不存在时允许 VM 启动，但不会隐式按工作目录补全。 */
        std::filesystem::path scriptRoot;
        /** @brief 当前 Project 开发期 `.venv` 的绝对根目录。 */
        std::filesystem::path virtualEnvironmentRoot;
    };

    class ScriptsSystem
    {
      public:
      public:
        static ScriptsSystem& Instance()
        {
            static ScriptsSystem instance;
            return instance;
        }

        /** @brief 使用当前 Project 的开发期虚拟环境初始化脚本 VM；两个路径都必须为绝对路径。 */
        bool Init(const ScriptRuntimeConfig& config);
        void Shutdown();
        bool IsInitialized() const
        {
            return guard != nullptr;
        }

      private:
        ScriptsSystem() = default;
        ~ScriptsSystem() = default;
        py::scoped_interpreter* guard = nullptr;
    };
} // namespace ChikaEngine::Scripts
