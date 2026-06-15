#include "ChikaEngine/Application.hpp"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/debug/log_system.h"

#include <charconv>
#include <memory>
#include <string_view>

namespace ChikaEngine::Game
{
    /**
     * @brief Standalone 游戏进程入口，只复用 Runtime Application 生命周期。
     *
     * 该入口不创建 Editor、Panel 或 UI Backend；`--smoke-frames` 仅用于自动验证
     * 初始化、主循环和逆序关闭路径，正式运行时保持为零并由窗口关闭事件退出。
     */
    class GameApplication final : public Engine::Application
    {
      public:
        explicit GameApplication(uint32_t smokeFrameCount) : m_smokeFrameCount(smokeFrameCount) {}

      protected:
        /**
         * @brief 提供 Standalone 窗口配置，后续 Project Descriptor 将覆盖这里的临时默认值。
         */
        Engine::EngineContextCreateInfo CreateEngineContextInfo() const override
        {
            Engine::EngineContextCreateInfo createInfo;
            createInfo.window.title = "Chika Game";
            createInfo.window.width = 1280;
            createInfo.window.height = 720;
            createInfo.window.isFullscreen = false;
            return createInfo;
        }

        /**
         * @brief 在 Smoke 模式达到指定帧数后请求退出，验证正常关闭而非强制终止。
         */
        void OnUpdate(float) override
        {
            if (m_smokeFrameCount > 0 && ++m_frameCount >= m_smokeFrameCount)
                RequestExit();
        }

      private:
        uint32_t m_smokeFrameCount = 0;
        uint32_t m_frameCount = 0;
    };

    /**
     * @brief 解析可选的 `--smoke-frames N`，无效输入回退为普通交互运行。
     */
    uint32_t ParseSmokeFrameCount(int argc, char** argv)
    {
        for (int index = 1; index + 1 < argc; ++index)
        {
            if (std::string_view(argv[index]) != "--smoke-frames")
                continue;

            uint32_t value = 0;
            const std::string_view text(argv[index + 1]);
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec == std::errc{} && result.ptr == text.data() + text.size())
                return value;
        }
        return 0;
    }
} // namespace ChikaEngine::Game

int main(int argc, char** argv)
{
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());
    ChikaEngine::Game::GameApplication application(ChikaEngine::Game::ParseSmokeFrameCount(argc, argv));
    return application.Run();
}
