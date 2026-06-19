#include "ChikaEngine/ScriptBindingRegistry.h"
#include "ChikaEngine/ScriptSystem.h"
#include "ChikaEngine/debug/log_macros.h"
#include <filesystem>

PYBIND11_EMBEDDED_MODULE(chika_engine, m)
{
    m.doc() = "ChikaEngine Module";
    ChikaEngine::Scripts::ScriptBindingRegistry::Instance().BindAll(m);
}

namespace ChikaEngine::Scripts
{
    namespace fs = std::filesystem;
    bool ScriptsSystem::Init(const std::filesystem::path& scriptRoot)
    {
        if (guard)
            return true;

        fs::path engineRoot = fs::current_path();

        fs::path venvPath = std::filesystem::current_path() / ".venv";
#if defined(_WIN32)
        fs::path pythonExe = venvPath / "Scripts" / "python.exe";
#else
        fs::path pythonExe = venvPath / "bin" / "python";
#endif

        if (!fs::exists(pythonExe))
        {
            LOG_ERROR("Scripts", "Fatal Error: Python executable not found");
            return false;
        }
        try
        {
            ChikaEngine::Scripts::InitAllPythonBindings();

            PyConfig config;
            PyConfig_InitPythonConfig(&config);
            const auto pythonHome = fs::path(CHIKA_PYTHON_HOME).wstring();
            const PyStatus homeStatus = PyConfig_SetString(&config, &config.home, pythonHome.c_str());
            if (PyStatus_Exception(homeStatus))
            {
                const std::string error = PyStatus_IsError(homeStatus) ? homeStatus.err_msg : "Failed to configure CPython home";
                PyConfig_Clear(&config);
                LOG_ERROR("Scripting", "Python configuration failed: {}", error);
                return false;
            }

            guard = new py::scoped_interpreter{ &config };

            py::module_ sys = py::module_::import("sys");
            py::list path = sys.attr("path");
            path.append(std::filesystem::absolute(scriptRoot).lexically_normal().string());
            LOG_INFO("Scripting", "Python Script Engine initialized successfully.");
            return true;
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("Scripting", "Python initialization failed: {}", exception.what());
            delete guard;
            guard = nullptr;
            return false;
        }
    }
    void ScriptsSystem::Shutdown()
    {
        if (!guard)
            return;
        delete guard;
        guard = nullptr;
    }
} // namespace ChikaEngine::Scripts
