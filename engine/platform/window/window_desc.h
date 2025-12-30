#include <string>
namespace ChikaEngine::Platform
{
    enum class WindowBackend
    {
        None,
        GLFW,
    };
    struct WindowDesc
    {
        std::string title = "Chika Engine";
        int width = 1280;
        int height = 720;
        bool vSync = false;
    };
} // namespace ChikaEngine::Platform