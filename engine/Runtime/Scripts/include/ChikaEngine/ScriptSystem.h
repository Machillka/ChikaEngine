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
#include <pybind11/embed.h>
namespace py = pybind11;
namespace ChikaEngine::Scripts
{
    class ScriptsSystem
    {
      public:
      public:
        static ScriptsSystem& Instance()
        {
            static ScriptsSystem instance;
            return instance;
        }

        void Init();
        void Shutdown();

      private:
        ScriptsSystem() = default;
        ~ScriptsSystem() = default;
        py::scoped_interpreter* guard = nullptr;
    };
} // namespace ChikaEngine::Scripts