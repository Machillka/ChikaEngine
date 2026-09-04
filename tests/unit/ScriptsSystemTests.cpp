#include "ChikaEngine/ScriptSystem.h"

#include <pybind11/pybind11.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    namespace py = pybind11;
    namespace Scripts = ChikaEngine::Scripts;

    int g_failures = 0;

    void Check(bool condition, const std::string& message)
    {
        if (condition)
            return;
        std::cerr << "FAILED: " << message << '\n';
        ++g_failures;
    }

    bool SameExistingPath(const fs::path& left, const fs::path& right)
    {
        std::error_code error;
        const bool equivalent = fs::equivalent(left, right, error);
        return !error && equivalent;
    }

    bool IsInside(const fs::path& child, const fs::path& parent)
    {
        std::error_code childError;
        std::error_code parentError;
        const fs::path canonicalChild = fs::weakly_canonical(child, childError);
        const fs::path canonicalParent = fs::weakly_canonical(parent, parentError);
        if (childError || parentError)
            return false;

        const fs::path relative = canonicalChild.lexically_relative(canonicalParent);
        return !relative.empty() && *relative.begin() != "..";
    }

    class CurrentPathGuard
    {
      public:
        explicit CurrentPathGuard(const fs::path& path) : m_previous(fs::current_path())
        {
            fs::current_path(path);
        }

        ~CurrentPathGuard()
        {
            std::error_code error;
            fs::current_path(m_previous, error);
        }

      private:
        fs::path m_previous;
    };
} // namespace

int main()
{
    const fs::path projectRoot = fs::path(CHIKA_PROJECT_ROOT_DIR);
    const fs::path virtualEnvironmentRoot = projectRoot / ".venv";
    const auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temporaryRoot = fs::temp_directory_path() / ("chika-scripts-contract-" + std::to_string(uniqueSuffix));
    const fs::path workingDirectory = temporaryRoot / "working directory without venv";
    const fs::path scriptRoot = temporaryRoot / "project scripts";

    std::error_code cleanupError;
    fs::remove_all(temporaryRoot, cleanupError);
    fs::create_directories(workingDirectory);
    fs::create_directories(scriptRoot);
    {
        std::ofstream probe(scriptRoot / "chika_script_probe.py");
        probe << "probe_value = 42\n";
    }

    {
        CurrentPathGuard currentPath(workingDirectory);
        Check(!fs::exists(fs::current_path() / ".venv"), "test working directory does not contain a virtual environment");

        const bool relativePathsAccepted = Scripts::ScriptsSystem::Instance().Init({
            .scriptRoot = "project scripts",
            .virtualEnvironmentRoot = ".venv",
        });
        Check(!relativePathsAccepted, "relative runtime paths are rejected instead of being resolved from the working directory");

        const bool missingEnvironmentAccepted = Scripts::ScriptsSystem::Instance().Init({
            .scriptRoot = scriptRoot,
            .virtualEnvironmentRoot = temporaryRoot / "missing-venv",
        });
        Check(!missingEnvironmentAccepted, "missing virtual environment is rejected");
        Check(!Scripts::ScriptsSystem::Instance().IsInitialized(), "failed initialization leaves no interpreter guard");

        const bool initialized = Scripts::ScriptsSystem::Instance().Init({
            .scriptRoot = scriptRoot,
            .virtualEnvironmentRoot = virtualEnvironmentRoot,
        });
        Check(initialized, "script system initializes from an explicit project virtual environment");
        Check(Scripts::ScriptsSystem::Instance().IsInitialized(), "successful initialization owns an interpreter guard");

        if (initialized)
        {
            try
            {
                py::module_ sys = py::module_::import("sys");
                const fs::path prefix = py::str(sys.attr("prefix")).cast<std::string>();
                const fs::path basePrefix = py::str(sys.attr("base_prefix")).cast<std::string>();
                Check(SameExistingPath(prefix, virtualEnvironmentRoot), "sys.prefix identifies the requested virtual environment");
                Check(!SameExistingPath(prefix, basePrefix), "embedded Python distinguishes the virtual environment from its base runtime");
                Check(sys.attr("dont_write_bytecode").cast<bool>(), "embedded Python does not write cache files into source asset directories");

                py::module_ jinja = py::module_::import("jinja2");
                const fs::path jinjaPath = py::str(jinja.attr("__file__")).cast<std::string>();
                Check(IsInside(jinjaPath, virtualEnvironmentRoot), "third-party package is imported from the requested virtual environment");

                py::module_::import("chika_engine");
                py::module_ probe = py::module_::import("chika_script_probe");
                Check(probe.attr("probe_value").cast<int>() == 42, "project script root is importable from an unrelated working directory");
            }
            catch (const std::exception& exception)
            {
                std::cerr << "FAILED: Python contract check threw: " << exception.what() << '\n';
                ++g_failures;
            }
        }

        Scripts::ScriptsSystem::Instance().Shutdown();
        Check(!Scripts::ScriptsSystem::Instance().IsInitialized(), "script system shuts down cleanly");
    }

    fs::remove_all(temporaryRoot, cleanupError);
    if (g_failures != 0)
    {
        std::cerr << g_failures << " script system check(s) failed\n";
        return 1;
    }

    std::cout << "Script system checks passed\n";
    return 0;
}
