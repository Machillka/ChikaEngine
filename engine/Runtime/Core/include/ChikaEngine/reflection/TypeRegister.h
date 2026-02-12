#pragma once

#include "ChikaEngine/reflection/ReflectionData.h"

#include <string>
#include <unordered_map>

namespace ChikaEngine::Reflection
{
    // 声明全局注册函数 通过 code rendering 实现
    void InitAllReflection();
    class TypeRegister
    {
      public:
        static TypeRegister& Instance();
        void RegisterClass(const ClassInfo& info);
        const ClassInfo* GetClass(const std::string& fullName);

      public:
        std::unordered_map<std::string, ClassInfo> _registry;
    };
} // namespace ChikaEngine::Reflection