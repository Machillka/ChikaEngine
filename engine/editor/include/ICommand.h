#pragma once

namespace ChikaEngine::Editor
{
    class ICommand
    {
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
    };
} // namespace ChikaEngine::Editor