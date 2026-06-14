#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ChikaEngine::Shader
{
    /**
     * @brief 描述 Shader Stage 的后端无关位掩码。
     *
     * 数值不与 Vulkan 常量绑定，避免 Asset、Material 和测试层依赖具体图形 API。
     */
    enum class ShaderStageMask : uint32_t
    {
        None = 0,
        Vertex = 1u << 0,
        Fragment = 1u << 1,
        Compute = 1u << 2,
    };

    /**
     * @brief 合并 Shader Stage 可见性位。
     */
    ShaderStageMask operator|(ShaderStageMask lhs, ShaderStageMask rhs);
    ShaderStageMask& operator|=(ShaderStageMask& lhs, ShaderStageMask rhs);
    bool HasStage(ShaderStageMask mask, ShaderStageMask stage);

    /**
     * @brief 描述 Shader 可声明的 Descriptor 类型。
     */
    enum class ShaderDescriptorType : uint32_t
    {
        UniformBuffer,
        StorageBuffer,
        CombinedImageSampler,
        SampledImage,
        StorageImage,
        Sampler,
    };

    /**
     * @brief 描述接口变量和 Buffer Member 的基础数值类型。
     */
    enum class ShaderValueType : uint32_t
    {
        Unknown,
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4,
        UInt,
        UInt2,
        UInt3,
        UInt4,
        Mat3,
        Mat4,
    };

    /**
     * @brief 保存一个反射 Buffer 成员的真实内存布局。
     *
     * Material 使用 offset/size 写入数据，而不是重新猜测 std140/std430 对齐。
     */
    struct ShaderBufferMember
    {
        std::string name;
        ShaderValueType type = ShaderValueType::Unknown;
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t arrayCount = 1;
        uint32_t arrayStride = 0;
        uint32_t matrixStride = 0;

        bool operator==(const ShaderBufferMember&) const = default;
    };

    /**
     * @brief 保存 Uniform/Storage Buffer 的块大小和成员布局。
     */
    struct ShaderBufferLayout
    {
        std::string name;
        uint32_t size = 0;
        std::vector<ShaderBufferMember> members;

        bool operator==(const ShaderBufferLayout&) const = default;
    };

    /**
     * @brief 保存一个 Descriptor 的稳定 set/binding/type 契约。
     */
    struct ShaderResourceBinding
    {
        std::string name;
        uint32_t set = 0;
        uint32_t binding = 0;
        ShaderDescriptorType type = ShaderDescriptorType::UniformBuffer;
        uint32_t arrayCount = 1;
        ShaderStageMask stages = ShaderStageMask::None;
        ShaderBufferLayout buffer;

        bool operator==(const ShaderResourceBinding&) const = default;
    };

    /**
     * @brief 保存 Push Constant 的实际范围和可见 Stage。
     */
    struct ShaderPushConstantRange
    {
        std::string name;
        uint32_t offset = 0;
        uint32_t size = 0;
        ShaderStageMask stages = ShaderStageMask::None;
        ShaderBufferLayout buffer;

        bool operator==(const ShaderPushConstantRange&) const = default;
    };

    /**
     * @brief 保存 Stage 输入变量，Vertex Stage 输入会用于生成 Vertex Layout。
     */
    struct ShaderVertexInput
    {
        std::string name;
        uint32_t location = 0;
        ShaderValueType type = ShaderValueType::Unknown;

        bool operator==(const ShaderVertexInput&) const = default;
    };

    /**
     * @brief 保存 Stage 输出变量，Fragment Stage 输出会用于校验 Attachment。
     */
    struct ShaderFragmentOutput
    {
        std::string name;
        uint32_t location = 0;
        ShaderValueType type = ShaderValueType::Unknown;

        bool operator==(const ShaderFragmentOutput&) const = default;
    };

    /**
     * @brief 保存单个 Shader Stage 的完整反射结果。
     */
    struct ShaderReflectionData
    {
        ShaderStageMask stage = ShaderStageMask::None;
        std::string entryPoint = "main";
        std::vector<ShaderResourceBinding> resources;
        std::vector<ShaderPushConstantRange> pushConstants;
        std::vector<ShaderVertexInput> inputs;
        std::vector<ShaderFragmentOutput> outputs;

        bool operator==(const ShaderReflectionData&) const = default;
    };

    /**
     * @brief 保存多个 Shader Stage 合并后创建 Pipeline 所需的唯一接口。
     */
    struct ShaderProgramInterface
    {
        std::vector<ShaderResourceBinding> resources;
        std::vector<ShaderPushConstantRange> pushConstants;
        std::vector<ShaderVertexInput> vertexInputs;
        std::vector<ShaderFragmentOutput> fragmentOutputs;
        uint64_t stableHash = 0;

        /**
         * @brief 按稳定 Shader 资源名称查找 Descriptor。
         */
        const ShaderResourceBinding* FindResource(std::string_view name) const;

        /**
         * @brief 按资源和成员名称查找 Reflection Buffer Member。
         */
        const ShaderBufferMember* FindBufferMember(std::string_view resourceName, std::string_view memberName) const;

        bool operator==(const ShaderProgramInterface&) const = default;
    };

    /**
     * @brief 返回 Shader Program 合并结果和可直接展示给用户的冲突说明。
     */
    struct ShaderProgramBuildResult
    {
        bool success = false;
        ShaderProgramInterface interface;
        std::vector<std::string> errors;
    };

    /**
     * @brief 对 Reflection 数据进行稳定排序。
     *
     * 导入顺序可能受编译器影响，排序后才能可靠序列化、比较和 Hash。
     */
    void NormalizeShaderReflection(ShaderReflectionData& reflection);

    /**
     * @brief 计算跨进程稳定的 Shader Interface Hash。
     *
     * 使用显式字段编码而不是 std::hash，确保不同运行和标准库实现得到相同结果。
     */
    uint64_t HashShaderReflection(const ShaderReflectionData& reflection);
    uint64_t HashShaderProgramInterface(const ShaderProgramInterface& interface);

    /**
     * @brief 合并多个 Stage，并在创建 Pipeline 前报告资源与 Stage 接口冲突。
     */
    ShaderProgramBuildResult BuildShaderProgramInterface(std::span<const ShaderReflectionData> stages);
} // namespace ChikaEngine::Shader
