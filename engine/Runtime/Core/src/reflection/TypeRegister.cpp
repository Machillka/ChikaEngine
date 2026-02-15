
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/reflection/ReflectionData.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include <iostream>
#include <string>
#include <unordered_map>

namespace ChikaEngine::Reflection
{
    TypeRegister& TypeRegister::Instance()
    {
        static TypeRegister instance;
        return instance;
    }

    void TypeRegister::RegisterClass(const ClassInfo& info)
    {
        if (_registryFullName.find(info.FullClassName) != _registryFullName.end())
        {
            LOG_WARN("Reflection", "Class {} already registered, skipping.", info.FullClassName);
            return;
        }
        _registryFullName[info.FullClassName] = info;
        _registry[info.Name] = info;
        LOG_INFO("Reflection", "Registed:{}", info.FullClassName);
    }

    const ClassInfo* TypeRegister::GetClassByFullName(const std::string& fullName)
    {
        auto it = _registryFullName.find(fullName);
        return it != _registryFullName.end() ? &it->second : nullptr;
    }

    const ClassInfo* TypeRegister::GetClassByName(const std::string& name)
    {
        auto it = _registry.find(name);
        std::cout << "Mother fucker!!!" << std::endl;
        return it != _registry.end() ? &it->second : nullptr;
    }
} // namespace ChikaEngine::Reflection