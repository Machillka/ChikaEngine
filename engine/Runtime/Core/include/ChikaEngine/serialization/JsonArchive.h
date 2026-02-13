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

    class JsonOutputArchive
    {
      public:
        // 用于编译期计算到底属于 read 还是 write mode
        static constexpr bool IsSaving = true;
        static constexpr bool IsLoading = false;

        JsonOutputArchive(IO::IStream& stream) : _stream(stream)
        {
            _stack.push(&_root);
        }
        // 析构的时候执行写入 不需要手动执行
        ~JsonOutputArchive()
        {
            std::string s = _root.dump(4);
            _stream.Write(s.data(), s.size());
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
            json& j = (*_stack.top())[name];
            if (j.is_null())
                j = json::object();
            _stack.push(&j);
        }

        void LeaveNode()
        {
            _stack.pop();
        }

        void EnterArray(const char* name, size_t size)
        {
            json* j = _stack.top();
            if (name)
                j = &(*j)[name];
            *j = json::array();
            _stack.push(j);
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

    class BinaryOutputArchive
    {
      public:
        // 定义一个栈帧结构
        struct StackFrame
        {
            json* node;        // 当前指向的 JSON 节点 (Object 或 Array)
            size_t arrayIndex; // 如果 node 是 Array，这里记录当前读到了第几个元素
        };

        static constexpr bool IsSaving = true;
        static constexpr bool IsLoading = false;

        BinaryOutputArchive(IO::IStream& stream) : _stream(stream)
        {
            if (!_stream.IsReadingMode())
            {
                LOG_ERROR("Serialization", "JsonInputArchive requires a stream in Reading Mode!");
                return;
            }

            // 一次性获得所有数据, 用于之后解析称 json 对象
            size_t len = _stream.GetLength();
            std::string str;
            str.resize(len);
            _stream.Read(str.data(), len);

            _root = json::parse(str);
            _stack.push({&_root, 0});
        }

        // 输出的时候忽略名字
        template <typename T> void operator()(const NVP<T>& nvp)
        {
            Serializer::Dispatch(*this, nullptr, nvp.Value);
        }

        template <typename T> void operator()(const T& val)
        {
            Serializer::Dispatch(*this, nullptr, const_cast<T&>(val));
        }

        void EnterNode(const char* name)
        {
            StackFrame& top = _stack.top();
            json* nextNode = nullptr;
            // 判断类型和名字
            if (top.node->is_object() && name)
            {
                nextNode = &(*top.node)[name];
            }
            else if (top.node->is_array())
            {
                // 如果在数组里进入一个对象
                nextNode = &(*top.node)[top.arrayIndex];
                top.arrayIndex++; // 索引后移
            }

            // 如果还是空
            if (!nextNode)
            {
                LOG_ERROR("Serialization", "Invalid JSON Node Access");
                return;
            }

            _stack.push({nextNode, 0});
        }

        void LeaveNode()
        {
            _stack.pop();
        }

        void EnterArray(const char* name, size_t& size)
        {
            StackFrame& top = _stack.top();
            json* arrNode = nullptr;

            if (name)
            {
                // 数组为 ( "array name": [...] )
                if (top.node->contains(name))
                {
                    arrNode = &(*top.node)[name];
                }
            }
            else
            {
                //  嵌套 ( [[...], [...]] )
                if (top.arrayIndex < top.node->size())
                {
                    arrNode = &(*top.node)[top.arrayIndex];
                    top.arrayIndex++;
                }
            }

            if (!arrNode || !arrNode->is_array())
            {
                // 处理实际上不是数组的情况
                size = 0;
                // 压入一个空数组指针防止 crash，或者抛出异常
                static json emptyArr = json::array();
                _stack.push({&emptyArr, 0});
                return;
            }

            // 赋值 size 允许 vector 对象resize
            size = arrNode->size();

            // 压栈
            _stack.push({arrNode, 0});
        }

        void LeaveArray()
        {
            _stack.pop();
        }

        // 读取具体数值
        template <typename T> void ProcessValue(const char* name, T& val)
        {
            StackFrame& top = _stack.top();

            // 有名字，尝试从 Object 中获取
            if (name)
            {
                if (top.node->is_object() && top.node->contains(name))
                {
                    val = top.node->at(name).get<T>();
                }
            }
            // 没名字，尝试从 Array 获取
            else
            {
                if (top.node->is_array() && top.arrayIndex < top.node->size())
                {
                    val = top.node->at(top.arrayIndex).get<T>();
                    top.arrayIndex++;
                }
            }
        }

      private:
        IO::IStream& _stream;
        std::stack<StackFrame> _stack;
        json _root;
    };
} // namespace ChikaEngine::Serialization