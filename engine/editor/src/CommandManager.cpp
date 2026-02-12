#include "CommandManager.h"
#include "ICommand.h"
#include <memory>

namespace ChikaEngine::Editor
{
    void CommandManager::ExecutedCommand(std::unique_ptr<ICommand> command)
    {
        if (!command)
            return;
        command->Execute();
        _history.push_back(std::move(command));
    }

    void CommandManager::Clear()
    {
        _history.clear();
    }

    bool CommandManager::CanUndo() const
    {
        return !_history.empty();
    }

    void CommandManager::Undo()
    {
        if (!CanUndo())
            return;
        _history.back()->Undo();
        _history.pop_back();
    }

} // namespace ChikaEngine::Editor