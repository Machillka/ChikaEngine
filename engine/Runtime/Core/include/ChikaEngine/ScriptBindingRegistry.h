#pragma once
#include <vector>
#include <functional>
#include <pybind11/pybind11.h>
#include "ChikaEngine/debug/log_macros.h"

namespace ChikaEngine::Scripts
{
    class ScriptBindingRegistry
    {
      public:
        using BindCallback = std::function<void(pybind11::module&)>;

        static ScriptBindingRegistry& Instance()
        {
            static ScriptBindingRegistry instance;
            return instance;
        }

        void Register(BindCallback callback, const char* name)
        {
            m_callbacks.push_back(callback);
            LOG_INFO("PythonBind", "Registered binding for: %s", name);
        }

        void BindAll(pybind11::module& m)
        {
            LOG_INFO("PythonBind", "Executing %zu python binding callbacks...", m_callbacks.size());
            for (auto& cb : m_callbacks)
            {
                cb(m);
            }
        }

      private:
        std::vector<BindCallback> m_callbacks;
    };

    void InitAllPythonBindings();
} // namespace ChikaEngine::Scripts