#include "debug/debugbreak.h"
#include "debug/log_system.h"

#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#endif
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
        // 将方法名称写作 Assert 并且 广播给所有sink
        LogSystem::Instance().Log(LogLevel::Error, "Assert", text);
        // 添加 Windows 弹窗
        // TODO: 重构一个窗口类 实现跨平台弹窗
#ifdef _WIN32
        std::string boxText = text;
        boxText += "\n\nAbort: exit program\nRetry: break into debugger\nIgnore: continue";
        int result =
            ::MessageBoxA(nullptr, boxText.c_str(), "Engine Assertion Failed", MB_ABORTRETRYIGNORE | MB_ICONERROR);
        if (result == IDABORT)
        {
            std::abort();
        }
        else if (result == IDRETRY)
        {
            CHIKA_DEBUG_BREAK();
        }
        else
        {
            return;
        }
#else
        CHIKA_DEBUG_BREAK();
#endif
    }
} // namespace ChikaEngine::Debug