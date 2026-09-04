#pragma once

#include "EditorContext.hpp"
#include <string>
namespace ChikaEngine::Editor
{

    class IEditorPanel
    {
      public:
        virtual ~IEditorPanel() = default;
        virtual void Initialize(EditorContext* context) = 0;
        virtual void Tick(float deltaTime) = 0; // logic
        virtual void OnImGuiRender() = 0;       // render ui
        virtual const std::string& GetName() const = 0;

        void SetActive(bool active)
        {
            _isActive = active;
        }
        bool IsActive() const
        {
            return _isActive;
        }

      protected:
        EditorContext* _context = nullptr;
        bool _isActive = true;
    };
} // namespace ChikaEngine::Editor