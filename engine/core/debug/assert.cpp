#include "debug/log_system.h"

#include <iostream>
#include <sstream>

namespace ChikaEngine::Debug
{
    // 搭建断言处理信息 并且
    void HandleAssert(const char* expr, const char* file, int line, const char* func, const char* msg)
    {
        std::ostringstream oss;
        oss << "Assertion failed!\n"
            << "Expression: " << expr << "\n"
            << "File: " << file << "\n"
            << "Line: " << line << "\n"
            << "Function: " << func << "\n";
        if (msg)
            oss << "Message: " << msg << "\n";
        std::string text = oss.str();
        // 将方法名称写作 Assert
        LogSystem::Instance().Log(LogLevel::Error, "Assert", text);
        std::cout << text;
    }
} // namespace ChikaEngine::Debug