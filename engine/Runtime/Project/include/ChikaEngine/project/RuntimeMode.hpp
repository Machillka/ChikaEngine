#pragma once

namespace ChikaEngine::Project
{
    /**
     * @brief 区分 Editor、源资产游戏和 Cooked 发布游戏的能力边界。
     */
    enum class RuntimeMode
    {
        Editor,
        DevelopmentGame,
        PackagedGame,
    };
} // namespace ChikaEngine::Project
