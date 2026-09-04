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
    namespace
    {
        fs::path PythonExecutable(const fs::path& virtualEnvironmentRoot)
        {
#if defined(_WIN32)
            return virtualEnvironmentRoot / "Scripts" / "python.exe";
#else
            return virtualEnvironmentRoot / "bin" / "python";
#endif
        }

        bool SetConfigPath(PyConfig& config, wchar_t** destination, const fs::path& value, const char* name)
        {
#if defined(_WIN32)
            const std::wstring wideValue = value.wstring();
            const PyStatus status = PyConfig_SetString(&config, destination, wideValue.c_str());
#else
            const std::string nativeValue = value.native();
            const PyStatus status = PyConfig_SetBytesString(&config, destination, nativeValue.c_str());
#endif
            if (!PyStatus_Exception(status))
                return true;

            const std::string error = PyStatus_IsError(status) && status.err_msg ? status.err_msg : "unknown CPython configuration error";
            LOG_ERROR("Scripting", "Failed to configure {} '{}': {}", name, value.string(), error);
            return false;
        }
    } // namespace

    bool ScriptsSystem::Init(const ScriptRuntimeConfig& runtimeConfig)
    {
        if (guard)
            return true;

        if (runtimeConfig.virtualEnvironmentRoot.empty())
        {
            LOG_ERROR("Scripting", "Virtual environment root is not configured");
            return false;
        }
        if (!runtimeConfig.virtualEnvironmentRoot.is_absolute() || runtimeConfig.scriptRoot.empty() || !runtimeConfig.scriptRoot.is_absolute())
        {
            LOG_ERROR("Scripting", "Script and virtual environment roots must be absolute project paths");
            return false;
        }

        const fs::path virtualEnvironmentRoot = runtimeConfig.virtualEnvironmentRoot.lexically_normal();
        const fs::path scriptRoot = runtimeConfig.scriptRoot.lexically_normal();
        const fs::path pythonExe = PythonExecutable(virtualEnvironmentRoot);
        const fs::path virtualEnvironmentConfig = virtualEnvironmentRoot / "pyvenv.cfg";

        if (!fs::exists(pythonExe))
        {
            LOG_ERROR("Scripting", "Virtual environment Python executable not found: {}", pythonExe.string());
            return false;
        }
        if (!fs::exists(virtualEnvironmentConfig))
        {
            LOG_ERROR("Scripting", "Virtual environment configuration not found: {}", virtualEnvironmentConfig.string());
            return false;
        }
        if (!fs::exists(scriptRoot))
            LOG_WARN("Scripting", "Project script directory does not exist yet: {}", scriptRoot.string());

        try
        {
            ChikaEngine::Scripts::InitAllPythonBindings();

            PyConfig config;
            PyConfig_InitPythonConfig(&config);
            config.install_signal_handlers = 0;
            config.parse_argv = 0;
            config.safe_path = 1;
            config.use_environment = 0;
            config.user_site_directory = 0;
            config.write_bytecode = 0;

            if (!SetConfigPath(config, &config.program_name, pythonExe, "Python program") || !SetConfigPath(config, &config.executable, pythonExe, "Python executable"))
            {
                PyConfig_Clear(&config);
                return false;
            }

            guard = new py::scoped_interpreter{ &config, 0, nullptr, false };

            fs::path actualPrefix;
            {
                py::module_ sys = py::module_::import("sys");
                actualPrefix = fs::path(py::str(sys.attr("prefix")).cast<std::string>());
                py::list path = sys.attr("path");
                path.append(scriptRoot.string());
            }

            std::error_code equivalentError;
            const bool usesExpectedEnvironment = fs::equivalent(actualPrefix, virtualEnvironmentRoot, equivalentError);
            if (equivalentError || !usesExpectedEnvironment)
            {
                LOG_ERROR("Scripting", "CPython initialized with unexpected prefix '{}'; expected virtual environment '{}'", actualPrefix.string(), virtualEnvironmentRoot.string());
                delete guard;
                guard = nullptr;
                return false;
            }

            LOG_INFO("Scripting", "Python Script Engine initialized with virtual environment '{}'.", virtualEnvironmentRoot.string());
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
