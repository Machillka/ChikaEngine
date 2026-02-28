#pragma once

#include "ChikaEngine/ResourceTypes.h"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionData.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include <type_traits>

namespace ChikaEngine::Serialization
{
    // 实现 name -> value 的键值对
    template <typename T> struct NVP
    {
        const char* Name;
        T& Value;
    };

    template <typename T> inline NVP<T> make_nvp(const char* name, T& value)
    {
        return {name, value};
    }

    template <typename T>
    concept BasicType = std::is_arithmetic_v<std::remove_cvref_t<T>> || std::is_same_v<std::remove_cvref_t<T>, std::string>;

    template <typename T>
    concept Reflectable = requires { std::remove_cvref_t<T>::GetClassName(); };

    template <typename T, typename Ar>
    concept CustomSerializable = requires(T& v, Ar& ar) { v.serialize(ar); };

    template <typename T>
    concept EnumType = std::is_enum_v<std::remove_cvref_t<T>>;

    // SFINAE 检测: 是否存在 serialize(Archive&) 方法, 如果有的话 直接调用即可
    template <typename T, typename Archive> class has_serialize_method
    {
        template <typename U> static auto test(int) -> decltype(std::declval<U>().serialize(std::declval<Archive&>()), std::true_type());
        template <typename> static std::false_type test(...);

      public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    // 检测是否是 vector 类型
    template <typename T> struct is_vector : std::false_type
    {
    };
    template <typename T, typename A> struct is_vector<std::vector<T, A>> : std::true_type
    {
    };

    // 分发器
    class Serializer
    {
      public:
        // 所有 archive 都会通过调用这个方法来进行类型的分发
        // TODO: 使用 20 的标准约束
        // template <typename Ar, typename T> static void Dispatch(Ar& ar, const char* name, T& value)
        // {
        //     using RawT = std::remove_cvref_t<T>;
        //     // 基础类型
        //     if constexpr (std::is_arithmetic_v<RawT>)
        //     {
        //         ar.ProcessValue(name, value);
        //     }
        //     // std::string
        //     else if constexpr (std::is_same_v<RawT, std::string>)
        //     {
        //         ar.ProcessValue(name, value);
        //     }
        //     // 枚举类型
        //     else if constexpr (std::is_enum_v<RawT>)
        //     {
        //         using Underlying = std::underlying_type_t<RawT>;
        //         if constexpr (Ar::IsSaving)
        //         {
        //             Underlying v = static_cast<Underlying>(value);
        //             ar.ProcessValue(name, v);
        //         }
        //         else
        //         {
        //             Underlying v;
        //             ar.ProcessValue(name, v);
        //             value = static_cast<RawT>(v);
        //         }
        //     }
        //     // vector 容器
        //     else if constexpr (is_vector<RawT>::value)
        //     {
        //         size_t size = value.size();
        //         // ar 需要实现数组的处理
        //         ar.EnterArray(name, size);

        //         if constexpr (Ar::IsLoading)
        //             value.resize(size);

        //         for (auto& item : value)
        //         {
        //             Dispatch(ar, nullptr, item); // 递归 实现嵌套数组, 数组元素没有名称
        //         }
        //         ar.LeaveArray();
        //     }
        //     // 如果是侵入式设计 序列化对象包含有一个自定义的 serialize 方法, 咱就是说直接调用
        //     else if constexpr (has_serialize_method<RawT, Ar>::value)
        //     {
        //         value.serialize(ar);
        //     }
        //     // 自动化处理 反射类型
        //     else
        //     {
        //         SerializeReflected(ar, name, value);
        //     }
        // }

        template <typename Ar, BasicType T> static void Dispatch(Ar& ar, const char* name, T& value)
        {
            ar.ProcessValue(name, value);
        }

        template <typename Ar, EnumType T> static void Dispatch(Ar& ar, const char* name, T& value)
        {
            using RawT = std::remove_cvref_t<T>;
            using Underlying = std::underlying_type_t<RawT>;
            if constexpr (Ar::IsSaving)
            {
                auto v = static_cast<Underlying>(value);
                ar.ProcessValue(name, v);
            }
            else
            {
                Underlying v;
                ar.ProcessValue(name, v);
                value = static_cast<RawT>(v);
            }
        }

        template <typename Ar, typename T>
            requires is_vector<std::remove_cvref_t<T>>::value
        static void Dispatch(Ar& ar, const char* name, T& value)
        {
            size_t size = value.size();
            ar.EnterArray(name, size);
            if constexpr (Ar::IsLoading)
                value.resize(size);
            for (auto& item : value)
            {
                Dispatch(ar, nullptr, item);
            }
            ar.LeaveArray();
        }

        template <typename Ar, typename T>
            requires CustomSerializable<T, Ar>
        static void Dispatch(Ar& ar, const char* name, T& value)
        {
            // 注意：这里强制转为非 const，因为 serialize 方法内部可能包含 Load 逻辑
            const_cast<std::remove_cvref_t<T>&>(value).serialize(ar);
        }

        template <typename Ar, typename T> static void Dispatch(Ar& ar, const char* name, T& value)
        {
            if constexpr (Reflectable<T>)
            {
                SerializeReflected(ar, name, value);
            }
            else
            {
                static_assert(sizeof(T) == 0, "Type is not serializable");
            }
        }

      private:
        // 检测是否有 GetReflectedClassName 方法
        template <typename T> struct has_get_reflected_classname
        {
            template <typename U> static auto test(int) -> decltype(std::declval<U>().GetReflectedClassName(), std::true_type());
            template <typename> static std::false_type test(...);

            static constexpr bool value = decltype(test<T>(0))::value;
        };

        template <typename Ar, typename T> static void SerializeReflected(Ar& ar, const char* name, T& instance)
        {
            // 调用 reflect body 中定义的获得名称的方法
            // TODO: 此处只是类名 而不是 fullPath
            using RawT = std::remove_cvref_t<T>;
            std::string className;
            if constexpr (has_get_reflected_classname<RawT>::value)
            {
                // 如果有 GetReflectedClassName 方法（如 Component 类），使用存储的字符串获取真实类名
                className = instance.GetReflectedClassName();
            }
            else
            {
                // 普通反射对象使用静态类名
                className = RawT::GetClassName();
            }

            const auto* classInfo = ChikaEngine::Reflection::TypeRegister::Instance().GetClassByName(className.c_str());
            if (!classInfo)
            {
                // 如果连反射信息都没有，这是一个无法序列化的对象
                LOG_ERROR("Serialization", "Can't serialze this object {}", name);
                return;
            }
            // 如果存在名字 就把 名字作为一个根节点
            if (name)
                ar.EnterNode(name);
            uint8_t* basePtr = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&instance));
            for (const auto& prop : classInfo->Properties)
            {
                // 计算属性在内存中的绝对地址
                // 指针运算：对象首地址 + 偏移量
                uint8_t* rawAddr = basePtr + prop.Offset;

                // 根据反射类型 (ReflectType) 还原类型并递归
                // WORKFLOW: 这里的 switch 必须覆盖 Reflection::ReflectType 枚举的所有值
                switch (prop.Type)
                {
                case Reflection::ReflectType::Int:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<int*>(rawAddr));
                    break;
                case Reflection::ReflectType::Float:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<float*>(rawAddr));
                    break;
                case Reflection::ReflectType::Bool:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<bool*>(rawAddr));
                    break;
                case Reflection::ReflectType::String:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<std::string*>(rawAddr));
                    break;

                case Reflection::ReflectType::MaterialHandle:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<Resource::MaterialHandle*>(rawAddr));
                    break;
                case Reflection::ReflectType::MeshHandle:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<Resource::MeshHandle*>(rawAddr));
                    break;

                // 特殊处理数学类型
                case Reflection::ReflectType::Vector3:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<Math::Vector3*>(rawAddr));
                    break;
                case Reflection::ReflectType::Quaternion:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<Math::Quaternion*>(rawAddr));
                    break;
                case Reflection::ReflectType::Transform:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<Framework::Transform*>(rawAddr));
                    break;
                default:
                    LOG_WARN("Serialization", "Unknown reflection type: {}", static_cast<int>(prop.Type));
                    break;
                }
            }

            if (name)
                ar.LeaveNode();
        }
    };

} // namespace ChikaEngine::Serialization