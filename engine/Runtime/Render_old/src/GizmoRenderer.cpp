#include "ChikaEngine/GizmoRenderer.h"
#include "ChikaEngine/debug/assert.h"
#include "ChikaEngine/debug/log_macros.h"
#include <stdexcept>

namespace ChikaEngine::Render
{
    static const char* GIZMO_VS = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec4 aColor;
        uniform mat4 u_ViewProjection;
        out vec4 vColor;
        void main() {
            gl_Position = u_ViewProjection * vec4(aPos, 1.0);
            vColor = aColor;
        }
    )";

    static const char* GIZMO_FS = R"(
        #version 330 core
        in vec4 vColor;
        out vec4 FragColor;
        void main() {
            FragColor = vColor;
        }
    )";

    void GizmoRenderer::Init(IRHIDevice* rhiDevice)
    {
        _rhiDevice = rhiDevice;
        CHIKA_ASSERT(_rhiDevice, "RHI is null");

        _pipeline.reset(_rhiDevice->CreatePipeline(GIZMO_VS, GIZMO_FS));
        if (!_pipeline)
        {
            throw std::runtime_error("Failed to create gizmo pipeline");
        }

        _vbo.reset(_rhiDevice->CreateVertexBuffer(MAX_VERTICES * sizeof(GizmoVertex), nullptr));
        if (!_vbo)
        {
            throw std::runtime_error("Failed to create gizmo vertex buffer");
        }

        _vao.reset(_rhiDevice->CreateVertexArray());
        if (!_vao)
        {
            throw std::runtime_error("Failed to create gizmo vertex array");
        }

        _rhiDevice->SetupGizmoVertexLayout(_vao.get(), _vbo.get());
        LOG_INFO("GizmoRenderer", "Gizmo renderer initialized successfully");
    }

    void GizmoRenderer::Shutdown()
    {

        _pipeline.reset();
        _vbo.reset();
        _vao.reset();
        _rhiDevice = nullptr;
        LOG_INFO("GizmoRenderer", "Gizmo renderer shutdown successfully");
    }

    void GizmoRenderer::Render(IRHIRenderTarget* target, const CameraData& camera, const std::vector<GizmoVertex>& lines)
    {
        if (lines.empty())
            return;

        if (!target)
        {
            LOG_ERROR("GizmoRenderer", "Render called with null render target");
            return;
        }

        if (!_rhiDevice)
        {
            LOG_ERROR("GizmoRenderer", "Render called but RHI device is null");
            return;
        }

        if (!_pipeline || !_vbo || !_vao)
        {
            LOG_ERROR("GizmoRenderer", "Gizmo rendering resources not initialized properly");
            return;
        }

        target->Bind();
        _pipeline->Bind();

        Math::Mat4 vp = camera.projectionMatrix * camera.viewMatrix;
        _pipeline->SetUniformMat4("u_ViewProjection", vp);

        std::size_t totalVertices = lines.size();
        std::size_t offset = 0;

        // 分批 处理
        while (totalVertices > 0)
        {
            std::size_t drawCount = std::min(totalVertices, static_cast<std::size_t>(MAX_VERTICES));

            if (offset >= lines.size())
            {
                LOG_ERROR("GizmoRenderer", "Offset exceeded vertex buffer size");
                break;
            }

            const GizmoVertex* dataPtr = lines.data() + offset;
            if (dataPtr == nullptr)
            {
                LOG_ERROR("GizmoRenderer", "Invalid vertex data pointer at offset {}", offset);
                break;
            }

            // DEFENSIVE: Validate we're not reading past the buffer
            if (offset + drawCount > lines.size())
            {
                LOG_WARN("GizmoRenderer", "Clamping draw count to prevent buffer overrun");
                drawCount = lines.size() - offset;
            }

            if (drawCount == 0)
                break;

            _rhiDevice->UpdateBufferData(_vbo.get(), drawCount * sizeof(GizmoVertex), dataPtr, 0);
            _rhiDevice->DrawLines(_vao.get(), static_cast<std::uint32_t>(drawCount), 0);

            offset += drawCount;
            totalVertices -= drawCount;
        }

        target->UnBind();
    }
} // namespace ChikaEngine::Render