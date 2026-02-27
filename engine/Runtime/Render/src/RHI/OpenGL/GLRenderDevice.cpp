#include "ChikaEngine/RHI/OpenGL/GLRenderDevice.h"
#include "ChikaEngine/Resource/MaterialPool.h"
#include "ChikaEngine/Resource/MeshPool.h"
#include "ChikaEngine/Resource/ShaderPool.h"
#include "ChikaEngine/Resource/TexturePool.h"
#include "ChikaEngine/render_device.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>

namespace ChikaEngine::Render
{
    GLRenderDevice::GLRenderDevice(IRHIDevice* device) : _glRHIDevice(device) {}
    GLRenderDevice::~GLRenderDevice() {}
    void GLRenderDevice::Init()
    {
        MeshPool::Init(_glRHIDevice);
        ShaderPool::Init(_glRHIDevice);
        MaterialPool::Init(_glRHIDevice);
        TexturePool::Init(_glRHIDevice);
        TextureCubePool::Init(_glRHIDevice);

        // 初始化天空盒的基础网格和Shader
        InitSkyboxResources();
    }
    void GLRenderDevice::InitSkyboxResources()
    {
        // TODO: 规避硬编码 写成预编译之类的 shader 和 obj
        float skyboxVertices[] = {// positions
                                  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

                                  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

                                  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

                                  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

                                  -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

                                  -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        int numVerts = 36;
        vertices.reserve(numVerts);
        indices.reserve(numVerts);

        for (int i = 0; i < numVerts; ++i)
        {
            Vertex v;

            v.position[0] = skyboxVertices[i * 3 + 0];
            v.position[1] = skyboxVertices[i * 3 + 1];
            v.position[2] = skyboxVertices[i * 3 + 2];

            v.normal = {0.0f, 0.0f, 0.0f};
            v.uv = {0.0f, 0.0f};

            vertices.push_back(v);
            indices.push_back(i);
        }

        Mesh meshData;
        meshData.vertices = vertices;
        meshData.indices = indices;

        _skyboxMesh = MeshPool::Create(meshData);

        const char* vsSrc = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            out vec3 TexCoords;
            uniform mat4 u_View;
            uniform mat4 u_Projection;
            void main() {
                TexCoords = aPos;
                mat4 view = mat4(mat3(u_View));
                vec4 pos = u_Projection * view * vec4(aPos, 1.0);
                gl_Position = pos.xyww;
            }
        )";

        const char* fsSrc = R"(
            #version 330 core
            out vec4 FragColor;
            in vec3 TexCoords;
            uniform samplerCube u_Skybox;
            void main() {
                FragColor = texture(u_Skybox, TexCoords);
            }
        )";

        Shader shader{vsSrc, fsSrc};
        _skyboxShader = ShaderPool::Create(shader);
    }
    void GLRenderDevice::BeginFrame()
    {
        _glRHIDevice->BeginFrame();
    }

    void GLRenderDevice::EndFrame()
    {
        _glRHIDevice->EndFrame();
    }

    void GLRenderDevice::DrawObject(const RenderObject& obj, const CameraData& camera)
    {
        // LOG_INFO("GLRenderDevice", "Drawing object mesh={} material={}", obj.mesh, obj.material);
        const RHIMesh& mesh = MeshPool::Get(obj.mesh);
        RHIMaterial& mat = MaterialPool::Get(obj.material);
        Math::Mat4 mvp = camera.projectionMatrix * camera.viewMatrix * obj.modelMat;
        mat.uniformMat4s["u_MVP"] = mvp;
        mat.uniformMat4s["u_Model"] = obj.modelMat;
        mat.uniformVec3s["u_CameraPos"] = {camera.position.x, camera.position.y, camera.position.z};
        MaterialPool::Apply(obj.material);
        _glRHIDevice->DrawIndexed(mesh);
    }

    void GLRenderDevice::DrawSkybox(TextureCubeHandle cubemap, const CameraData& camera)
    {
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        auto& shader = ShaderPool::Get(_skyboxShader);
        shader.pipeline->Bind();

        Math::Mat4 view = camera.viewMatrix;

        shader.pipeline->SetUniformMat4("u_View", view);
        shader.pipeline->SetUniformMat4("u_Projection", camera.projectionMatrix);

        const auto& cubeData = TextureCubePool::Get(cubemap);

        shader.pipeline->SetUniformTextureCube("u_Skybox", cubeData.texture, 0);

        const RHIMesh& mesh = MeshPool::Get(_skyboxMesh);
        _glRHIDevice->DrawIndexed(mesh);

        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    void GLRenderDevice::Shutdown()
    {
        MeshPool::Reset();
        ShaderPool::Reset();
        MaterialPool::Reset();
        TexturePool::Reset();
    }
} // namespace ChikaEngine::Render