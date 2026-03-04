#pragma once

#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/reflection/ReflectionData.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include <type_traits>

namespace ChikaEngine::Serialization
{
    using namespace ChikaEngine::Reflection;

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
    concept BasicType = std::is_arithmetic_v<std::remove_cvref_t<T>>;

    template <typename T>
    concept Reflectable = requires {
        { std::remove_cvref_t<T>::GetClassName() } -> std::convertible_to<const char*>;
    };

    template <typename T, typename Ar>
    concept CustomSerializable = requires(T& v, Ar& ar) { v.serialize(ar); };

    template <typename T>
    concept EnumType = std::is_enum_v<std::remove_cvref_t<T>>;

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

    template <typename T> struct has_get_reflected_classname
    {
        template <typename U> static auto test(int) -> decltype(std::declval<U>().GetReflectedClassName(), std::true_type());
        template <typename> static std::false_type test(...);

        static constexpr bool value = decltype(test<T>(0))::value;
    };

    // 分发器
    class Serializer
    {
      public:
        template <typename Ar, BasicType T> static void Dispatch(Ar& ar, const char* name, T& value)
        {
            using NonConstT = std::remove_const_t<T>;
            ar.ProcessValue(name, const_cast<NonConstT&>(value));
        }

        template <typename Ar, typename T>
            requires std::same_as<std::remove_cvref_t<T>, std::string>
        static void Dispatch(Ar& ar, const char* name, T& value)
        {
            ar.ProcessValue(name, const_cast<std::string&>(value));
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
                const_cast<RawT&>(value) = static_cast<RawT>(v);
            }
        }

        template <typename Ar, typename T>
            requires is_vector<std::remove_cvref_t<T>>::value
        static void Dispatch(Ar& ar, const char* name, T& value)
        {
            using NonConstVec = std::remove_const_t<T>;
            auto& mutableVec = const_cast<NonConstVec&>(value);

            size_t size = mutableVec.size();
            ar.EnterArray(name, size);

            if constexpr (Ar::IsLoading)
            {
                mutableVec.resize(size);
            }

            for (auto& item : mutableVec)
            {
                Dispatch(ar, nullptr, item);
            }
            ar.LeaveArray();
        }

        // NOTE: 把 container 直接遍历存储 因为 container<T> 递归处理类型 T

        template <typename Ar, typename T>
            requires CustomSerializable<T, Ar>
        static void Dispatch(Ar& ar, const char* name, T& value)
        {
            if (name)
                ar.EnterNode(name);
            const_cast<std::remove_cvref_t<T>&>(value).serialize(ar);
            if (name)
                ar.LeaveNode();
        }

        template <typename Ar, typename T> static void Dispatch(Ar& ar, const char* name, T& value)
        {
            if constexpr (Reflectable<T>)
            {
                // 进入反射处理流程
                SerializeReflectedObject(ar, name, value);
            }
            else
            {
                // 编译期报错
                static_assert(sizeof(T) == 0, "[Serializer] Type is not serializable. Did you forget to regist it");
            }
        }

      private:
        template <typename Ar, typename T> static void SerializeReflectedObject(Ar& ar, const char* name, T& instance)
        {
            using RawT = std::remove_cvref_t<T>;
            std::string className;
            if constexpr (has_get_reflected_classname<RawT>::value)
            {
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
                LOG_ERROR("Serialization", "Reflection data missing for class: {}", className);
                return;
            }

            if (name)
                ar.EnterNode(name);
            else
                ar.EnterNode(nullptr);

            // void* rawInstance = const_cast<uint8_t*>(static_cast<const uint8_t*>(&instance));
            uint8_t* rawInstance = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&instance));
            // 递归拆解
            SerializeRuntimeFields(ar, rawInstance, classInfo);

            ar.LeaveNode();
        }

        // TODO: 对于参与反射的类型 直接使用 python 生成一个强类型的 serialize 方法
        template <typename Ar> static void SerializeRuntimeFields(Ar& ar, uint8_t* instanceBase, const ClassInfo* classInfo)
        {
            for (const auto& prop : classInfo->Properties)
            {
                uint8_t* fieldAddr = instanceBase + prop.Offset;

                switch (prop.Type)
                {

                case ReflectType::Int:
                    ar.ProcessValue(prop.Name.c_str(), *reinterpret_cast<int*>(fieldAddr));
                    break;
                case ReflectType::Float:
                    ar.ProcessValue(prop.Name.c_str(), *reinterpret_cast<float*>(fieldAddr));
                    break;
                case ReflectType::Bool:
                    ar.ProcessValue(prop.Name.c_str(), *reinterpret_cast<bool*>(fieldAddr));
                    break;
                case ReflectType::String:
                    ar.ProcessValue(prop.Name.c_str(), *reinterpret_cast<std::string*>(fieldAddr));
                    break;

                // 子类也是 封闭后的类型
                case ReflectType::Object:
                {

                    // 孩子在得到反射类型的时候 raw type 是 full name
                    const auto* subClassInfo = TypeRegister::Instance().GetClassByFullName(prop.TypeName);
                    if (subClassInfo)
                    {
                        ar.EnterNode(prop.Name.c_str());
                        SerializeRuntimeFields(ar, fieldAddr, subClassInfo);
                        ar.LeaveNode();
                    }
                    else
                    {
                        LOG_ERROR("Serialization", "Unknown object type '{}' in field '{}'", prop.TypeName, prop.Name);
                    }
                    break;
                }

                // TODO: 做 STL 容器特化 -> 做一个 foreach 的访问器
                case ReflectType::Container:
                {
                    LOG_WARN("Serialization", "Serializing an array");
                    break;
                }

                default:
                    // 忽略未知类型或 Void
                    break;
                }
            }
        }
    };
} // namespace ChikaEngine::Serialization