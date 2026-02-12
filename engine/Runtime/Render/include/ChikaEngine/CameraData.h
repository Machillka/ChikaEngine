#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
namespace ChikaEngine::Render
{

    // 用于解耦 Camera 之需要传入 camera 数据就好
    struct CameraData
    {
        Math::Mat4 viewMatrix;
        Math::Mat4 projectionMatrix;
        Math::Vector3 position;
    };
} // namespace ChikaEngine::Render