#include "GlfwTimeBackend.h"

#include <GLFW/glfw3.h>

namespace ChikaEngine::Time
{
    double GlfwTimeBackend::GetCurrentTimeSeconds() const
    {
        return glfwGetTime();
    }
} // namespace ChikaEngine::Time