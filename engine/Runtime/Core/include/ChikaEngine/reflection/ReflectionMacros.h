#pragma once

// 在编译发才计算 算一种编译器”代码生成“,所以不需要实现类似“precompile”的机制
namespace ChikaEngine::Reflection
{
    template <typename T> class Reflector;
}

#if defined(__REFLECTION_PARSER__)

#define MCLASS(CLASS_NAME, ...) class __attribute__((annotate("reflect-class" #__VA_ARGS__))) CLASS_NAME
#define MFIELD(...) __attribute__((annotate("reflect-field" #__VA_ARGS__)))
#define MFUNCTION(...) __attribute__((annotate("reflect-function" #__VA_ARGS__)))
#else
// 空实现
#define MCLASS(CLASS_NAME, ...) class CLASS_NAME
#define MFIELD(...)
#define MFUNCTION(...)
#endif

namespace ChikaEngine::Reflection
{
} // namespace ChikaEngine::Reflection
#define REFLECTION_BODY(CLASS_NAME) \
    friend class ::ChikaEngine::Reflection::Reflector<CLASS_NAME>; \
\
  public: \
    static const char* GetClassName() \
    { \
        return #CLASS_NAME; \
    }