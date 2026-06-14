#include "ChikaEngine/ShaderReflection.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <spirv_reflect.h>

namespace ChikaEngine::Asset
{
    namespace
    {
        using json = nlohmann::json;

        /**
         * @brief 将 SPIRV-Reflect Stage 转换为后端无关 Stage Mask。
         */
        Shader::ShaderStageMask ToStageMask(SpvReflectShaderStageFlagBits stage)
        {
            switch (stage)
            {
            case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
                return Shader::ShaderStageMask::Vertex;
            case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
                return Shader::ShaderStageMask::Fragment;
            case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
                return Shader::ShaderStageMask::Compute;
            default:
                return Shader::ShaderStageMask::None;
            }
        }

        /**
         * @brief 将支持的 SPIR-V Descriptor 类型转换为引擎契约。
         */
        bool ToDescriptorType(SpvReflectDescriptorType type, Shader::ShaderDescriptorType& output)
        {
            switch (type)
            {
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                output = Shader::ShaderDescriptorType::UniformBuffer;
                return true;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                output = Shader::ShaderDescriptorType::StorageBuffer;
                return true;
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                output = Shader::ShaderDescriptorType::CombinedImageSampler;
                return true;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                output = Shader::ShaderDescriptorType::SampledImage;
                return true;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                output = Shader::ShaderDescriptorType::StorageImage;
                return true;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                output = Shader::ShaderDescriptorType::Sampler;
                return true;
            default:
                return false;
            }
        }

        /**
         * @brief 将接口变量格式转换为 Shader Value Type。
         */
        Shader::ShaderValueType ToValueType(SpvReflectFormat format)
        {
            switch (format)
            {
            case SPV_REFLECT_FORMAT_R32_SFLOAT:
                return Shader::ShaderValueType::Float;
            case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
                return Shader::ShaderValueType::Float2;
            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
                return Shader::ShaderValueType::Float3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
                return Shader::ShaderValueType::Float4;
            case SPV_REFLECT_FORMAT_R32_SINT:
                return Shader::ShaderValueType::Int;
            case SPV_REFLECT_FORMAT_R32G32_SINT:
                return Shader::ShaderValueType::Int2;
            case SPV_REFLECT_FORMAT_R32G32B32_SINT:
                return Shader::ShaderValueType::Int3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
                return Shader::ShaderValueType::Int4;
            case SPV_REFLECT_FORMAT_R32_UINT:
                return Shader::ShaderValueType::UInt;
            case SPV_REFLECT_FORMAT_R32G32_UINT:
                return Shader::ShaderValueType::UInt2;
            case SPV_REFLECT_FORMAT_R32G32B32_UINT:
                return Shader::ShaderValueType::UInt3;
            case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
                return Shader::ShaderValueType::UInt4;
            default:
                return Shader::ShaderValueType::Unknown;
            }
        }

        /**
         * @brief 从 Block Variable 数值特征推导 Material 可写入的基础类型。
         */
        Shader::ShaderValueType ToValueType(const SpvReflectBlockVariable& variable)
        {
            const auto* type = variable.type_description;
            if (!type)
                return Shader::ShaderValueType::Unknown;
            const auto flags = type->type_flags;
            if ((flags & SPV_REFLECT_TYPE_FLAG_MATRIX) != 0)
            {
                if (variable.numeric.matrix.column_count == 3 && variable.numeric.matrix.row_count == 3)
                    return Shader::ShaderValueType::Mat3;
                if (variable.numeric.matrix.column_count == 4 && variable.numeric.matrix.row_count == 4)
                    return Shader::ShaderValueType::Mat4;
                return Shader::ShaderValueType::Unknown;
            }

            const uint32_t count = std::max(variable.numeric.vector.component_count, 1u);
            const bool isFloat = (flags & SPV_REFLECT_TYPE_FLAG_FLOAT) != 0;
            const bool isInt = (flags & SPV_REFLECT_TYPE_FLAG_INT) != 0;
            const bool isSigned = variable.numeric.scalar.signedness != 0;
            if (isFloat)
                return static_cast<Shader::ShaderValueType>(static_cast<uint32_t>(Shader::ShaderValueType::Float) + count - 1);
            if (isInt && isSigned)
                return static_cast<Shader::ShaderValueType>(static_cast<uint32_t>(Shader::ShaderValueType::Int) + count - 1);
            if (isInt)
                return static_cast<Shader::ShaderValueType>(static_cast<uint32_t>(Shader::ShaderValueType::UInt) + count - 1);
            return Shader::ShaderValueType::Unknown;
        }

        /**
         * @brief 递归拍平 Buffer 成员，同时保留 SPIR-V 给出的绝对偏移。
         */
        void AppendMembers(const SpvReflectBlockVariable& block, std::string_view prefix, std::vector<Shader::ShaderBufferMember>& output)
        {
            for (uint32_t i = 0; i < block.member_count; ++i)
            {
                const SpvReflectBlockVariable& member = block.members[i];
                const std::string name = prefix.empty() ? (member.name ? member.name : "") : std::string(prefix) + "." + (member.name ? member.name : "");
                if (member.member_count > 0)
                {
                    AppendMembers(member, name, output);
                    continue;
                }

                uint32_t arrayCount = 1;
                for (uint32_t dimension = 0; dimension < member.array.dims_count; ++dimension)
                    arrayCount *= member.array.dims[dimension];
                output.push_back({
                    .name = name,
                    .type = ToValueType(member),
                    .offset = member.absolute_offset,
                    .size = member.size,
                    .arrayCount = arrayCount,
                    .arrayStride = member.array.stride,
                    .matrixStride = member.numeric.matrix.stride,
                });
            }
        }

        /**
         * @brief 将 SPIRV-Reflect Block 转换为可持久化布局。
         */
        Shader::ShaderBufferLayout ToBufferLayout(const char* name, const SpvReflectBlockVariable& block)
        {
            Shader::ShaderBufferLayout layout{
                .name = name ? name : "",
                .size = block.padded_size > 0 ? block.padded_size : block.size,
            };
            AppendMembers(block, "", layout.members);
            return layout;
        }

        /**
         * @brief 序列化 Buffer Layout，保持 Sidecar 格式独立于 C++ ABI。
         */
        json SerializeBuffer(const Shader::ShaderBufferLayout& buffer)
        {
            json members = json::array();
            for (const auto& member : buffer.members)
            {
                members.push_back({
                    { "name", member.name },
                    { "type", static_cast<uint32_t>(member.type) },
                    { "offset", member.offset },
                    { "size", member.size },
                    { "arrayCount", member.arrayCount },
                    { "arrayStride", member.arrayStride },
                    { "matrixStride", member.matrixStride },
                });
            }
            return { { "name", buffer.name }, { "size", buffer.size }, { "members", std::move(members) } };
        }

        /**
         * @brief 反序列化 Buffer Layout，并拒绝缺失的关键布局字段。
         */
        Shader::ShaderBufferLayout DeserializeBuffer(const json& value)
        {
            Shader::ShaderBufferLayout buffer{
                .name = value.value("name", ""),
                .size = value.at("size").get<uint32_t>(),
            };
            for (const auto& item : value.value("members", json::array()))
            {
                buffer.members.push_back({
                    .name = item.at("name").get<std::string>(),
                    .type = static_cast<Shader::ShaderValueType>(item.at("type").get<uint32_t>()),
                    .offset = item.at("offset").get<uint32_t>(),
                    .size = item.at("size").get<uint32_t>(),
                    .arrayCount = item.value("arrayCount", 1u),
                    .arrayStride = item.value("arrayStride", 0u),
                    .matrixStride = item.value("matrixStride", 0u),
                });
            }
            return buffer;
        }

        /**
         * @brief 判断接口变量是否为 SPIR-V Builtin，Builtin 不属于 Pipeline 可连接接口。
         */
        bool IsBuiltin(const SpvReflectInterfaceVariable& variable)
        {
            return variable.built_in >= 0 || variable.location == std::numeric_limits<uint32_t>::max();
        }
    } // namespace

    bool ShaderReflection::Reflect(std::span<const uint8_t> spirv, Shader::ShaderReflectionData& output, std::string& error)
    {
        SpvReflectShaderModule module{};
        const SpvReflectResult createResult = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &module);
        if (createResult != SPV_REFLECT_RESULT_SUCCESS)
        {
            error = "SPIRV-Reflect failed to create module: " + std::to_string(createResult);
            return false;
        }

        output = {};
        output.stage = ToStageMask(module.shader_stage);
        output.entryPoint = module.entry_point_name ? module.entry_point_name : "main";
        if (output.stage == Shader::ShaderStageMask::None)
        {
            error = "Unsupported shader stage";
            spvReflectDestroyShaderModule(&module);
            return false;
        }

        uint32_t bindingCount = 0;
        spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr);
        std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
        spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data());
        for (const SpvReflectDescriptorBinding* binding : bindings)
        {
            Shader::ShaderDescriptorType type;
            if (!ToDescriptorType(binding->descriptor_type, type))
            {
                error = "Unsupported descriptor type at set " + std::to_string(binding->set) + ", binding " + std::to_string(binding->binding);
                spvReflectDestroyShaderModule(&module);
                return false;
            }
            output.resources.push_back({
                .name = binding->name ? binding->name : "",
                .set = binding->set,
                .binding = binding->binding,
                .type = type,
                .arrayCount = std::max(binding->count, 1u),
                .stages = output.stage,
                .buffer = ToBufferLayout(binding->name, binding->block),
            });
        }

        uint32_t pushCount = 0;
        spvReflectEnumeratePushConstantBlocks(&module, &pushCount, nullptr);
        std::vector<SpvReflectBlockVariable*> pushBlocks(pushCount);
        spvReflectEnumeratePushConstantBlocks(&module, &pushCount, pushBlocks.data());
        for (const SpvReflectBlockVariable* block : pushBlocks)
        {
            output.pushConstants.push_back({
                .name = block->name ? block->name : "",
                .offset = block->offset,
                .size = block->padded_size > 0 ? block->padded_size : block->size,
                .stages = output.stage,
                .buffer = ToBufferLayout(block->name, *block),
            });
        }

        uint32_t inputCount = 0;
        spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);
        std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
        spvReflectEnumerateInputVariables(&module, &inputCount, inputs.data());
        for (const SpvReflectInterfaceVariable* input : inputs)
        {
            if (!IsBuiltin(*input))
                output.inputs.push_back({ input->name ? input->name : "", input->location, ToValueType(input->format) });
        }

        uint32_t outputCount = 0;
        spvReflectEnumerateOutputVariables(&module, &outputCount, nullptr);
        std::vector<SpvReflectInterfaceVariable*> outputs(outputCount);
        spvReflectEnumerateOutputVariables(&module, &outputCount, outputs.data());
        for (const SpvReflectInterfaceVariable* variable : outputs)
        {
            if (!IsBuiltin(*variable))
                output.outputs.push_back({ variable->name ? variable->name : "", variable->location, ToValueType(variable->format) });
        }

        spvReflectDestroyShaderModule(&module);
        Shader::NormalizeShaderReflection(output);
        return true;
    }

    bool ShaderReflection::Save(const std::filesystem::path& path, const Shader::ShaderReflectionData& reflection, std::string& error)
    {
        try
        {
            json resources = json::array();
            for (const auto& resource : reflection.resources)
            {
                resources.push_back({
                    { "name", resource.name },
                    { "set", resource.set },
                    { "binding", resource.binding },
                    { "type", static_cast<uint32_t>(resource.type) },
                    { "arrayCount", resource.arrayCount },
                    { "stages", static_cast<uint32_t>(resource.stages) },
                    { "buffer", SerializeBuffer(resource.buffer) },
                });
            }

            json pushConstants = json::array();
            for (const auto& range : reflection.pushConstants)
            {
                pushConstants.push_back({
                    { "name", range.name },
                    { "offset", range.offset },
                    { "size", range.size },
                    { "stages", static_cast<uint32_t>(range.stages) },
                    { "buffer", SerializeBuffer(range.buffer) },
                });
            }

            const auto serializeVariables = [](const auto& variables)
            {
                json values = json::array();
                for (const auto& variable : variables)
                    values.push_back({ { "name", variable.name }, { "location", variable.location }, { "type", static_cast<uint32_t>(variable.type) } });
                return values;
            };

            json document{
                { "version", 1 }, { "stage", static_cast<uint32_t>(reflection.stage) }, { "entryPoint", reflection.entryPoint }, { "stableHash", Shader::HashShaderReflection(reflection) }, { "resources", std::move(resources) }, { "pushConstants", std::move(pushConstants) }, { "inputs", serializeVariables(reflection.inputs) }, { "outputs", serializeVariables(reflection.outputs) },
            };

            std::ofstream file(path, std::ios::trunc);
            file << document.dump(2) << '\n';
            if (!file.good())
            {
                error = "Failed to write reflection sidecar: " + path.string();
                return false;
            }
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }

    bool ShaderReflection::Load(const std::filesystem::path& path, Shader::ShaderReflectionData& reflection, std::string& error)
    {
        try
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
                error = "Reflection sidecar does not exist: " + path.string();
                return false;
            }
            const json document = json::parse(file);
            reflection = {};
            reflection.stage = static_cast<Shader::ShaderStageMask>(document.at("stage").get<uint32_t>());
            reflection.entryPoint = document.value("entryPoint", "main");
            for (const auto& item : document.value("resources", json::array()))
            {
                reflection.resources.push_back({
                    .name = item.at("name").get<std::string>(),
                    .set = item.at("set").get<uint32_t>(),
                    .binding = item.at("binding").get<uint32_t>(),
                    .type = static_cast<Shader::ShaderDescriptorType>(item.at("type").get<uint32_t>()),
                    .arrayCount = item.value("arrayCount", 1u),
                    .stages = static_cast<Shader::ShaderStageMask>(item.at("stages").get<uint32_t>()),
                    .buffer = DeserializeBuffer(item.at("buffer")),
                });
            }
            for (const auto& item : document.value("pushConstants", json::array()))
            {
                reflection.pushConstants.push_back({
                    .name = item.at("name").get<std::string>(),
                    .offset = item.at("offset").get<uint32_t>(),
                    .size = item.at("size").get<uint32_t>(),
                    .stages = static_cast<Shader::ShaderStageMask>(item.at("stages").get<uint32_t>()),
                    .buffer = DeserializeBuffer(item.at("buffer")),
                });
            }

            const auto deserializeVariables = [](const json& values, auto& output)
            {
                for (const auto& item : values)
                    output.push_back({ item.at("name").get<std::string>(), item.at("location").get<uint32_t>(), static_cast<Shader::ShaderValueType>(item.at("type").get<uint32_t>()) });
            };
            deserializeVariables(document.value("inputs", json::array()), reflection.inputs);
            deserializeVariables(document.value("outputs", json::array()), reflection.outputs);
            Shader::NormalizeShaderReflection(reflection);
            if (document.contains("stableHash") && document.at("stableHash").get<uint64_t>() != Shader::HashShaderReflection(reflection))
            {
                error = "Reflection sidecar hash does not match serialized interface";
                return false;
            }
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }

    std::filesystem::path ShaderReflection::SidecarPath(const std::filesystem::path& spirvPath)
    {
        return spirvPath.string() + ".reflection.json";
    }
} // namespace ChikaEngine::Asset
