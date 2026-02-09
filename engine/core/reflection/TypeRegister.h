#pragma once

#include "debug/log_macros.h"
#include "reflection/ReflectionData.h"
#include <string>
#include <unordered_map>

namespace ChikaEngine::Reflection
{
    // 声明全局注册函数 通过 code rendering 实现
    void InitAllReflection();
    class TypeRegister
    {
      public:
        static TypeRegister& Instance()
        {
            static TypeRegister instance;
            return instance;
        }

        void RegisterClass(const ClassInfo& info)
        {
            _registry[info.FullClassName] = info;
            LOG_INFO("Reflection", "Registed:{}", info.FullClassName);
        }

        const ClassInfo* GetClass(const std::string& fullName)
        {
            auto it = _registry.find(fullName);
            return it != _registry.end() ? &it->second : nullptr;
        }

      public:
        std::unordered_map<std::string, ClassInfo> _registry;
    };
} // namespace ChikaEngine::Reflection