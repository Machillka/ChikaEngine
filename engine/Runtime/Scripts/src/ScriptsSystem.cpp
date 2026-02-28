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
    py::scoped_interpreter* guard = nullptr;
    namespace fs = std::filesystem;
    void ScriptsSystem::Init()
    {
        LOG_INFO("Scripting", "Initializing uv environment...");

        int ret = std::system("uv sync");
        if (ret != 0)
        {
            LOG_ERROR("Scripting", "uv sync failed!");
            return;
        }
        fs::path engineRoot = fs::current_path();

        fs::path venvPath = std::filesystem::current_path() / ".venv";
#if defined(_WIN32)
        fs::path pythonExe = engineRoot / "Scripts" / "python.exe";
#else
        fs::path pythonExe = venvPath / "bin" / "python";
#endif

        if (!fs::exists(pythonExe))
        {
            LOG_ERROR("Scripts", "Fatal Error: Python executable not found");
            return;
        }
        static std::wstring wPythonExe = pythonExe.wstring();
        Py_SetProgramName(wPythonExe.c_str());
        Py_IgnoreEnvironmentFlag = 1;
        // std::wstring pythonHome = venvPath.wstring();
        // Py_SetPythonHome(pythonHome.c_str());
        ChikaEngine::Scripts::InitAllPythonBindings();
        guard = new py::scoped_interpreter{};

        // py::module_ sys = py::module_::import("sys");
        // py::list path = sys.attr("path");
        // path.append(std::filesystem::current_path().string());

        py::module_ sys = py::module_::import("sys");
        py::list path = sys.attr("path");
        path.append((engineRoot / "Assets" / "Scripts").string());
        LOG_INFO("Scripting", "Python Script Engine initialized successfully.");
        LOG_INFO("Scripts", (engineRoot / "Assets" / "Scripts").string());
    }
    void ScriptsSystem::Shutdown()
    {
        delete guard;
        guard = nullptr;
    }
} // namespace ChikaEngine::Scripts