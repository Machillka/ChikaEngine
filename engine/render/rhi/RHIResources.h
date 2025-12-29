#include "math/mat4.h"
#include "math/vector3.h"
namespace ChikaEngine::Render
{
    // 顶点缓冲区
    class IRHIBuffer
    {
      public:
        virtual ~IRHIBuffer() = default;
    };

    class IRHIVertexArray
    {
      public:
        virtual ~IRHIVertexArray() = default;
    };

    class IRHIShader
    {
      public:
        virtual ~IRHIShader() = default;
        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;
        virtual void SetMat4(const char* name, const Math::Mat4& m) const = 0;
        virtual void SetVec3(const char* name, const Math::Vector3& v) const = 0;
    };

    // 渲染管线抽象
    class IRHIPipeline
    {
      public:
        virtual ~IRHIPipeline() = default;
        virtual void Bind() const = 0;
        virtual void UnBind() const = 0;
    };
} // namespace ChikaEngine::Render