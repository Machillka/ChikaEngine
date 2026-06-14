#include "ChikaEngine/shader/ShaderInterface.hpp"
#include "ChikaEngine/ShaderReflection.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

namespace
{
    namespace Shader = ChikaEngine::Shader;

    int g_failures = 0;

    /**
     * @brief 记录断言失败并继续执行，以便一次测试展示完整接口回归。
     */
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    /**
     * @brief 创建测试用 Uniform Buffer Binding，减少各测试重复布局样板。
     */
    Shader::ShaderResourceBinding MakeSceneBinding(Shader::ShaderStageMask stages)
    {
        return {
            .name = "scene",
            .set = 0,
            .binding = 0,
            .type = Shader::ShaderDescriptorType::UniformBuffer,
            .arrayCount = 1,
            .stages = stages,
            .buffer = {
                .name = "scene",
                .size = 80,
                .members = {
                    { .name = "viewProjection", .type = Shader::ShaderValueType::Mat4, .offset = 0, .size = 64, .matrixStride = 16 },
                    { .name = "cameraPosition", .type = Shader::ShaderValueType::Float4, .offset = 64, .size = 16 },
                },
            },
        };
    }

    /**
     * @brief 验证 Reflection Hash 不受工具枚举顺序影响。
     */
    void TestStableHashAndNormalization()
    {
        Shader::ShaderReflectionData first{
            .stage = Shader::ShaderStageMask::Vertex,
            .resources = {
                { .name = "material", .set = 1, .binding = 0, .type = Shader::ShaderDescriptorType::UniformBuffer, .stages = Shader::ShaderStageMask::Vertex },
                MakeSceneBinding(Shader::ShaderStageMask::Vertex),
            },
            .inputs = {
                { "normal", 1, Shader::ShaderValueType::Float3 },
                { "position", 0, Shader::ShaderValueType::Float3 },
            },
        };
        Shader::ShaderReflectionData second = first;
        std::ranges::reverse(second.resources);
        std::ranges::reverse(second.inputs);

        Check(Shader::HashShaderReflection(first) == Shader::HashShaderReflection(second), "reflection hash is stable across enumeration order");
        Shader::NormalizeShaderReflection(first);
        Shader::NormalizeShaderReflection(second);
        Check(first == second, "normalization creates comparable reflection data");
    }

    /**
     * @brief 验证 Program Builder 合并 Stage 可见性并连接 Vertex/Fragment 接口。
     */
    void TestProgramInterfaceMerge()
    {
        const Shader::ShaderReflectionData vertex{
            .stage = Shader::ShaderStageMask::Vertex,
            .resources = { MakeSceneBinding(Shader::ShaderStageMask::Vertex) },
            .inputs = { { "position", 0, Shader::ShaderValueType::Float3 } },
            .outputs = { { "worldPosition", 0, Shader::ShaderValueType::Float3 } },
        };
        const Shader::ShaderReflectionData fragment{
            .stage = Shader::ShaderStageMask::Fragment,
            .resources = { MakeSceneBinding(Shader::ShaderStageMask::Fragment) },
            .inputs = { { "worldPosition", 0, Shader::ShaderValueType::Float3 } },
            .outputs = { { "color", 0, Shader::ShaderValueType::Float4 } },
        };

        const std::array stages{ vertex, fragment };
        const Shader::ShaderProgramBuildResult result = Shader::BuildShaderProgramInterface(stages);
        Check(result.success, "compatible shader stages merge");
        Check(result.interface.resources.size() == 1, "shared descriptor merges once");
        Check(Shader::HasStage(result.interface.resources[0].stages, Shader::ShaderStageMask::Vertex), "merged descriptor keeps vertex visibility");
        Check(Shader::HasStage(result.interface.resources[0].stages, Shader::ShaderStageMask::Fragment), "merged descriptor keeps fragment visibility");
        Check(result.interface.stableHash != 0, "program interface exposes stable hash");
    }

    /**
     * @brief 验证冲突 Descriptor 和不匹配 Stage 接口会在 Pipeline 创建前失败。
     */
    void TestProgramInterfaceConflict()
    {
        Shader::ShaderReflectionData vertex{
            .stage = Shader::ShaderStageMask::Vertex,
            .resources = { MakeSceneBinding(Shader::ShaderStageMask::Vertex) },
            .outputs = { { "worldPosition", 0, Shader::ShaderValueType::Float3 } },
        };
        Shader::ShaderReflectionData fragment{
            .stage = Shader::ShaderStageMask::Fragment,
            .resources = { MakeSceneBinding(Shader::ShaderStageMask::Fragment) },
            .inputs = { { "worldPosition", 0, Shader::ShaderValueType::Float4 } },
        };
        fragment.resources[0].type = Shader::ShaderDescriptorType::StorageBuffer;

        const std::array stages{ vertex, fragment };
        const Shader::ShaderProgramBuildResult result = Shader::BuildShaderProgramInterface(stages);
        Check(!result.success, "conflicting shader stages fail");
        Check(result.errors.size() >= 2, "builder reports descriptor and stage interface conflicts");
    }

    /**
     * @brief 验证当前引擎 Shader 导入产物都能组成合法 Program Interface。
     */
    void TestCurrentShaderReflectionProducts()
    {
        const auto load = [](const char* path)
        {
            Shader::ShaderReflectionData reflection;
            std::string error;
            Check(ChikaEngine::Asset::ShaderReflection::Load(path, reflection, error), error.c_str());
            return reflection;
        };

        const Shader::ShaderReflectionData staticVertex = load("Assets/Shaders/test.vert.spv.reflection.json");
        const Shader::ShaderReflectionData skinnedVertex = load("Assets/Shaders/skinned.vert.spv.reflection.json");
        const Shader::ShaderReflectionData fullscreenVertex = load("Assets/Shaders/fullscreen.vert.spv.reflection.json");
        const Shader::ShaderReflectionData forwardFragment = load("Assets/Shaders/test.frag.spv.reflection.json");
        const Shader::ShaderReflectionData gbufferFragment = load("Assets/Shaders/gbuffer.frag.spv.reflection.json");
        const Shader::ShaderReflectionData deferredFragment = load("Assets/Shaders/deferred_lighting.frag.spv.reflection.json");

        const std::array staticForwardStages{ staticVertex, forwardFragment };
        const std::array skinnedForwardStages{ skinnedVertex, forwardFragment };
        const std::array staticGBufferStages{ staticVertex, gbufferFragment };
        const std::array deferredStages{ fullscreenVertex, deferredFragment };
        Check(Shader::BuildShaderProgramInterface(staticForwardStages).success, "static forward shader interface builds");
        Check(Shader::BuildShaderProgramInterface(skinnedForwardStages).success, "skinned forward shader interface builds");
        Check(Shader::BuildShaderProgramInterface(staticGBufferStages).success, "gbuffer shader interface builds");
        Check(Shader::BuildShaderProgramInterface(deferredStages).success, "deferred lighting shader interface builds");
        Check(staticVertex.inputs.size() == 3, "static vertex reflection removes unused skinned attributes");
        Check(skinnedVertex.inputs.size() == 5, "skinned vertex reflection keeps bone attributes");
        Check(gbufferFragment.outputs.size() == 3, "gbuffer reflection exposes all attachments");
    }
} // namespace

int main()
{
    TestStableHashAndNormalization();
    TestProgramInterfaceMerge();
    TestProgramInterfaceConflict();
    TestCurrentShaderReflectionProducts();

    if (g_failures != 0)
        std::cerr << g_failures << " shader interface test(s) failed\n";
    return g_failures == 0 ? 0 : 1;
}
