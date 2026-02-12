
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
        if (_registry.find(info.FullClassName) != _registry.end())
        {
            LOG_WARN("Reflection", "Class {} already registered, skipping.", info.FullClassName);
            return;
        }
        _registry[info.FullClassName] = info;
        LOG_INFO("Reflection", "Registed:{}", info.FullClassName);
    }

    const ClassInfo* TypeRegister::GetClass(const std::string& fullName)
    {
        auto it = _registry.find(fullName);
        return it != _registry.end() ? &it->second : nullptr;
    }

} // namespace ChikaEngine::Reflection