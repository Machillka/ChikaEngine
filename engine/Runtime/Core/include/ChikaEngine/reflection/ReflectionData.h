#pragma once

#include <algorithm>
#include <any>
#include <concepts>
#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace ChikaEngine::Reflection
{
    //  范型接口签名
    using PropertySetter = std::function<void(void* instance, const void* value)>;
    using PropertyGetter = std::function<void(void* instance, void* outValue)>;

    // 方法调用的后端, 在用户调用层使用模板来实现不同参数的展开
    using MethodInvoker = std::function<void(void* instance, const std::vector<std::any>& args)>;

    // WORKFLOW: 添加新的类型
    // enum class ReflectType
    // {
    //     Int,
    //     Float,
    //     Bool,
    //     String,
    //     Vector3,
    //     Quaternion,
    //     MeshHandle,
    //     MaterialHandle,
    //     Transform,
    //     Unknown
    // };

    // 测试用
    enum class ReflectType
    {
        Unknown = 0,
        Void,
        Bool,
        Int,
        Float,
        String,
        Object,
        Container
    };

    template <typename T>
    concept IsString = std::same_as<std::remove_cvref_t<T>, std::string>;

    template <typename T>
    concept IsContainer = requires(T a) {
        a.begin();
        a.end();
        a.size();
    } && !IsString<T>;

    // 通过模板获取 类型 / 指针 / 引用类
    template <typename T> struct TypeResolver
    {
        // 移除 const/volatile/reference
        using RawType = std::remove_cvref_t<T>;

        static constexpr ReflectType GetType()
        {
            if constexpr (std::is_same_v<RawType, void>)
                return ReflectType::Void;
            else if constexpr (std::is_same_v<RawType, bool>)
                return ReflectType::Bool;
            else if constexpr (std::is_integral_v<RawType>)
                return ReflectType::Int;
            else if constexpr (std::is_floating_point_v<RawType>)
                return ReflectType::Float;
            else if constexpr (IsString<RawType>)
                return ReflectType::String;
            else if constexpr (IsContainer<RawType>)
                return ReflectType::Container;
            else if constexpr (std::is_class_v<RawType>)
                return ReflectType::Object;
            else
                return ReflectType::Unknown;
        }

        static constexpr bool IsPointer = std::is_pointer_v<T>;
        static constexpr bool IsReference = std::is_reference_v<T>;
        static constexpr bool IsConst = std::is_const_v<std::remove_reference_t<T>>;
    };

    struct PropertyInfo
    {
        std::string Name;     // 字段名 "m_Health"
        ReflectType Type;     // 枚举类型 (Int, Float, Object...)
        std::string TypeName; // RawType 类型名的字符串类型 比如 int / Transform / ...
        size_t Offset;
        bool IsPointer; // 标记是否是 指针 / 引用 / 常量类型
        bool IsReference;
        bool IsConst;
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
        std::function<void*()> Constructor = nullptr;

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