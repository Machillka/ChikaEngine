#include "IEditorPanel.h"

namespace ChikaEngine::Editor
{
    class ImguiContentBrowserPanel : public IEditorPanel
    {
      public:
        ImguiContentBrowserPanel();
        const char* Name() const override
        {
            return "Content Browser";
        }
        void OnRender(UIContext& ctx) override;

      private:
        // 存储 browser 的路径
        std::filesystem::path _currentDirectory;
        char _newScriptName[128] = "MyScript";
    };

} // namespace ChikaEngine::Editor