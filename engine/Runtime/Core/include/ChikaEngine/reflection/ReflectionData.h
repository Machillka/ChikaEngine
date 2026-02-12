#pragma once

#include <algorithm>
#include <any>
#include <functional>
#include <string>
#include <vector>

namespace ChikaEngine::Reflection
{
    //  范型接口签名
    using PropertySetter = std::function<void(void* instance, const void* value)>;
    using PropertyGetter = std::function<void(void* instance, void* outValue)>;

    // 方法调用的后端, 在用户调用层使用模板来实现不同参数的展开
    using MethodInvoker = std::function<void(void* instance, const std::vector<std::any>& args)>;

    enum class ReflectType
    {
        Int,
        Float,
        Bool,
        String,
        Vector3,
        Quaternion,
        Unknown
    };

    struct PropertyInfo
    {
        std::string Name;
        ReflectType Type;
        PropertyGetter Get;
        PropertySetter Set;
    };

    struct FunctionInfo
    {
        std::string Name;
        MethodInvoker InvokeRaw; // 后端实现
        template <typename... Args> void Invoke(void* instance, Args&&... args) const
        {
            std::vector<std::any> argList;
            // 使用折叠表达式
            (argList.push_back(std::any(std::forward<Args>(args))), ...);
            InvokeRaw(instance, argList);
        }
    };

    struct ClassInfo
    {
        std::string FullClassName; // 包含命名空间的名字
        std::string Name;
        std::vector<PropertyInfo> Properties;
        std::vector<FunctionInfo> Functions;

        const PropertyInfo* FindProperty(const std::string& propName) const
        {
            auto it = std::find_if(Properties.begin(), Properties.end(), [&propName](const PropertyInfo& p) { return p.Name == propName; });

            if (it != Properties.end())
                return &(*it);

            return nullptr;
        }

        const FunctionInfo* FindFunction(const std::string& funcName) const
        {
            for (const auto& f : Functions)
                if (f.Name == funcName)
                    return &f;
            return nullptr;
        }
    };

} // namespace ChikaEngine::Reflection