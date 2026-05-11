#include "ChikaEngine/subsystem/RenderSubsystem.h"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/component/Animator.hpp"

namespace ChikaEngine::Framework
{
    RenderSubsystem::RenderSubsystem(Scene* ownerScene, Render::Renderer* renderInstance) : _ownerScene(ownerScene), _renderer(renderInstance)
    {
        _assetMgr = _renderer->GetAssetManager();
        if (_assetMgr == nullptr)
        {
            LOG_ERROR("Render Subsystem", "null of Asset Manager");
        }

        _resourceMgr = _renderer->GetResourceManager();
        if (_resourceMgr == nullptr)
        {
            LOG_ERROR("Render Subsystem", "null of Resource Manager");
        }
    }

    void RenderSubsystem::Tick(float deltaTime)
    {
        // LOG_INFO("Render Subsystem", "Ticking...");
        _drawCommands.clear();
        CollectDrawCommands();

        // TODO: 封装到单独组件
        using namespace Math;
        Vector3 lightPos(5.0f, 8.0f, 5.0f);
        Mat4 lightView = Mat4::LookAt(lightPos, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
        Mat4 lightProj = Mat4::Perspective(3.1415926f / 3.0f, 1.0f, 0.1f, 100.0f);
        lightProj(1, 1) *= -1.0f;
        lightProj(2, 2) = -100.0f / (100.0f - 0.1f);
        lightProj(2, 3) = -(100.0f * 0.1f) / (100.0f - 0.1f);
        Mat4 lightVP = lightProj * lightView;

        Vector3 camPos(0.0f, 4.0f, 8.0f);
        Mat4 cameraView = Mat4::LookAt(camPos, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
        Mat4 cameraProj = Mat4::Perspective(3.1415926f / 4.0f, float(1024) / float(720), 0.1f, 100.0f);
        cameraProj(1, 1) *= -1.0f;
        cameraProj(2, 2) = -100.0f / (100.0f - 0.1f);
        cameraProj(2, 3) = -(100.0f * 0.1f) / (100.0f - 0.1f);
        Mat4 cameraVP = cameraProj * cameraView;

        // _renderer->SubmitCamera(cameraVP, camPos);
        _renderer->SubmitLight(lightVP, lightPos);
        _renderer->SubmitDrawCommands(_drawCommands);
        // LOG_INFO("Render Subsystem", "Tickending...");
    }

    void RenderSubsystem::Cleanup() {}

    void RenderSubsystem::CollectDrawCommands()
    {

        const auto& gos = _ownerScene->GetAllGameobjects();
        // LOG_INFO("Render Subsystem", "numbers = {}", gos.size());
        for (const auto& go : gos)
        {
            if (!go->IsActive())
                continue;

            auto meshComp = go->GetComponent<MeshRenderer>();

            if (!meshComp || !meshComp->IsEnabled())
                continue;

            // 重载
            meshComp->ResolveAssets(*_assetMgr);

            auto meshAsset = meshComp->GetMeshAsset();
            auto matAsset = meshComp->GetMaterialAsset();

            if (!meshAsset.IsValid() || !matAsset.IsValid())
                continue;

            // NOTE: 感觉放到 mesh renderer component 中 标记 dirty 的时候上传就好
            auto meshHandle = _resourceMgr->UploadMesh(meshAsset);
            auto matHandle = _resourceMgr->UploadMaterial(matAsset);

            Render::DrawCommand cmd{};
            cmd.materialHandle = matHandle;
            cmd.meshHandle = meshHandle;

            if (go->transform)
                cmd.model = go->transform->GetWorldMat();

            // 添加蒙皮动画处理
            auto animator = go->GetComponent<Animator>();
            if (animator && !animator->finalMatrices.empty())
            {
                // 直接拿到 System 算好的矩阵上传 GPU
                Render::BufferHandle boneUBO = _resourceMgr->UploadBoneMatrices(animator->finalMatrices, meshComp->GetBoneUBO());

                meshComp->SetBoneUBO(boneUBO);
                cmd.boneUBO = boneUBO;
                cmd.isSkinned = true;
            }
            else
            {
                cmd.isSkinned = false;
            }

            _drawCommands.push_back(cmd);
        }
    }

} // namespace ChikaEngine::Framework