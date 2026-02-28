#pragma once

#include "ChikaEngine/io/IStream.h"
#include "ChikaEngine/serialization/Access.h"
#include "ChikaEngine/debug/log_macros.h"
#include "nlohmann/json.hpp"
#include <stack>
#include <string>
namespace ChikaEngine::Serialization
{
    using json = nlohmann::json;

    class JsonSaveArchive
    {
      public:
        static constexpr bool IsSaving = true;
        static constexpr bool IsLoading = false;

        JsonSaveArchive(IO::IStream& stream) : _stream(stream)
        {
            _stack.push(&_root);
        }
        // 析构的时候执行写入 不需要手动执行
        ~JsonSaveArchive()
        {
            std::string s = _root.dump(4);
            _stream.Write(s.data(), s.size());
            LOG_DEBUG("Fuck", "执行析构函数");
        }

        // 接受键值对 ar(make_nvp(name, value))
        template <typename T> void operator()(const NVP<T>& nvp)
        {
            Serializer::Dispatch(*this, nvp.Name, nvp.Value);
        }
        // 接受直接 ar (name, value)
        template <typename T> void operator()(const char* name, T& value)
        {
            Serializer::Dispatch(*this, name, value);
        }
        // 接受简单类型的单数值
        template <typename T> void operator()(const T& val)
        {
            Serializer::Dispatch(*this, nullptr, const_cast<T&>(val));
        }

        // Duck Interface
        void EnterNode(const char* name)
        {
            // json& j = (*_stack.top())[name];
            // if (j.is_null())
            //     j = json::object();
            // _stack.push(&j);

            json* top = _stack.top();

            if (top->is_array())
            {
                top->push_back(json::object());
                _stack.push(&top->back());
            }
            else
            {
                if (name)
                {
                    json& j = (*top)[name];
                    if (j.is_null())
                        j = json::object();
                    _stack.push(&j);
                }
                else
                {
                    LOG_ERROR("JsonSaveArchive", "Trying to enter a node without name in an Object context!");
                }
            }
        }

        void LeaveNode()
        {
            _stack.pop();
        }

        void EnterArray(const char* name, size_t size)
        {
            // json* j = _stack.top();
            // if (name)
            //     j = &(*j)[name];
            // *j = json::array();
            // _stack.push(j);
            json* top = _stack.top();
            if (top->is_array())
            {
                top->push_back(json::array());
                _stack.push(&top->back());
            }
            else
            {
                if (name)
                {
                    json& j = (*top)[name];
                    j = json::array();
                    _stack.push(&j);
                }
                else
                {
                    LOG_ERROR("JsonSaveArchive", "Trying to enter array without name in an Object context!");
                }
            }
        }
        void LeaveArray()
        {
            _stack.pop();
        }

        template <typename T> void ProcessValue(const char* name, const T& val)
        {
            json* j = _stack.top();
            if (j->is_array())
                j->push_back(val);
            else if (name)
                (*j)[name] = val;
        }

      private:
        IO::IStream& _stream; // 维护一个 io 流
        json _root;
        std::stack<json*> _stack; // 维护树状信息
    };
} // namespace ChikaEngine::Serialization