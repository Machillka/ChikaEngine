#pragma once

#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionData.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include <iostream>
#include <ostream>
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
        template <typename Ar, typename T> static void Dispatch(Ar& ar, const char* name, T& value)
        {
            // 基础类型
            if constexpr (std::is_arithmetic_v<T>)
            {
                std::cout << "简单类型捕捉" << std::endl;
                ar.ProcessValue(name, value);
            }
            // std::string
            else if constexpr (std::is_same_v<T, std::string>)
            {
                ar.ProcessValue(name, value);
            }
            // 枚举类型
            else if constexpr (std::is_enum_v<T>)
            {
                using Underlying = std::underlying_type_t<T>;
                if constexpr (Ar::IsSaving)
                {
                    Underlying v = static_cast<Underlying>(value);
                    ar.ProcessValue(name, v);
                }
                else
                {
                    Underlying v;
                    ar.ProcessValue(name, v);
                    value = static_cast<T>(v);
                }
            }
            // vector 容器
            else if constexpr (is_vector<T>::value)
            {
                size_t size = value.size();
                // ar 需要实现数组的处理
                ar.EnterArray(name, size);

                if constexpr (Ar::IsLoading)
                    value.resize(size);

                for (auto& item : value)
                {
                    Serialize(ar, nullptr, item); // 递归 实现嵌套数组, 数组元素没有名称
                }
                ar.LeaveArray();
            }
            // 如果是侵入式设计 序列化对象包含有一个自定义的 serialize 方法, 咱就是说直接调用
            else if constexpr (has_serialize_method<T, Ar>::value)
            {
                value.serialize(ar);
            }
            // 自动化处理 反射类型
            else
            {
                SerializeReflected(ar, name, value);
            }
        }

      private:
        template <typename Ar, typename T> static void SerializeReflected(Ar& ar, const char* name, T& instance)
        {
            // 调用 reflect body 中定义的获得名称的方法
            // TODO: 此处只是类名 而不是 fullPath
            using namespace std;
            cout << "Name " << name << endl;
            const auto* classInfo = ChikaEngine::Reflection::TypeRegister::Instance().GetClassByName(T::GetClassName());
            if (!classInfo)
            {
                // 如果连反射信息都没有，这是一个无法序列化的对象
                LOG_ERROR("Serialization", "Can't serialze this object {}", name);
                return;
            }
            // 如果存在名字 就把 名字作为一个根节点
            if (name)
                ar.EnterNode(name);

            for (const auto& prop : classInfo->Properties)
            {
                // 计算属性在内存中的绝对地址
                // 指针运算：对象首地址 + 偏移量
                uint8_t* rawAddr = reinterpret_cast<uint8_t*>(&instance) + prop.Offset;

                // 根据反射类型 (ReflectType) 还原类型并递归
                // WORKFLOW: 这里的 switch 必须覆盖 Reflection::ReflectType 枚举的所有值
                switch (prop.Type)
                {
                case Reflection::ReflectType::Int:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<int*>(rawAddr));
                    break;
                case Reflection::ReflectType::Float:
                    std::cout << prop.Name.c_str() << "Fuck" << std::endl;
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<float*>(rawAddr));
                    break;
                case Reflection::ReflectType::Bool:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<bool*>(rawAddr));
                    break;
                case Reflection::ReflectType::String:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<std::string*>(rawAddr));
                    break;

                // 特殊处理数学类型
                case Reflection::ReflectType::Vector3:
                    // 比如 vec3 是已经自定义过的反射类型 所以会被递归调用拆解
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<Math::Vector3*>(rawAddr));
                    break;
                case Reflection::ReflectType::Quaternion:
                    Dispatch(ar, prop.Name.c_str(), *reinterpret_cast<Math::Quaternion*>(rawAddr));
                    break;

                default:
                    break;
                }
            }

            if (name)
                ar.LeaveNode();
        }
    };

} // namespace ChikaEngine::Serialization