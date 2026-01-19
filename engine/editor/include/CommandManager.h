#pragma once
#include "ICommand.h"

#include <deque>
#include <memory>

namespace ChikaEngine::Editor
{
    class CommandManager
    {
      public:
        void ExecutedCommand(std::unique_ptr<ICommand> command);
        void Undo();
        // TODO: 实现编辑器重做的效果
        // void Redo();
        void Clear();
        bool CanUndo() const;
        // bool CanRedo() const;

      private:
        std::deque<std::unique_ptr<ICommand>> _history;
    };
} // namespace ChikaEngine::Editor