/*!
 * @file TypeRegister.cpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-02-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/reflection/ReflectionData.h"
#include "ChikaEngine/reflection/TypeRegister.h"

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
        return it != _registry.end() ? &it->second : nullptr;
    }

    void* TypeRegister::CreateInstanceByName(const std::string& name)
    {
        auto it = _registry.find(name);
        if (it != _registry.end() && it->second.Constructor != nullptr)
        {
            return it->second.Constructor();
        }
        return nullptr;
    }

} // namespace ChikaEngine::Reflection