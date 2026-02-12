#pragma once

namespace ChikaEngine::Debug
{
    // 断言处理接口 下面使用宏定义化简使用
    void HandleAssert(const char* expr, const char* file, int line, const char* func, const char* msg);
} // namespace ChikaEngine::Debug

// 1. Debug 模式：
// - 当 expr 为 false 时：调用 HandleAssert，弹窗 + 中断到调试器
// - 当 expr 为 true 时：什么都不做
// 2. Release 模式：
// - CHIKA_ASSERT：完全移除，避免运行时开销
// - CHIKA_VERIFY：仍然执行 expr（常用于函数调用返回值检查），但不会触发断言逻辑

// 得到第二个参数 用于自动推断宏展开类型
#define CHIKA_GET_SECOND_ARG(arg1, arg2, arg3, ...) arg3
// 根据参数选择使用类型
#define CHIKA_HAS_MSG(...) CHIKA_GET_SECOND_ARG(__VA_ARGS__, 2, 1, 0)

// 仅断言
#define CHIKA_ASSERT_1(expr)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            ::ChikaEngine::Debug::HandleAssert(#expr, __FILE__, __LINE__, __func__, nullptr);                          \
        }                                                                                                              \
    } while (0)
// 断言+输出 msg
#define CHIKA_ASSERT_2(expr, msg)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            ::ChikaEngine::Debug::HandleAssert(#expr, __FILE__, __LINE__, __func__, msg);                              \
        }                                                                                                              \
    } while (0)

//
#define CHIKA_ASSERT_CHOOSER(count) CHIKA_ASSERT_##count
#define CHIKA_ASSERT_EXPAND(count) CHIKA_ASSERT_CHOOSER(count)

#ifdef CHIKA_DEBUG
#define CHIKA_ASSERT(...) CHIKA_ASSERT_EXPAND(CHIKA_HAS_MSG(__VA_ARGS__))(__VA_ARGS__)
#define CHIKA_VERIFY(expr)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            ::ChikaEngine::Debug::HandleAssert(#expr, __FILE__, __LINE__, __func__, "VERIFY failed");                  \
        }                                                                                                              \
    } while (0)
#else
// Release 下完全移除断言
#define CHIKA_ASSERT(...)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)sizeof(__VA_ARGS__);                                                                                     \
    } while (0)
#define CHIKA_VERIFY(expr)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)(expr);                                                                                                  \
    } while (0)
#endif