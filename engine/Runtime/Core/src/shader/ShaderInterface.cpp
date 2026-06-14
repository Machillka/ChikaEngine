#include "ChikaEngine/shader/ShaderInterface.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <tuple>
#include <type_traits>

namespace ChikaEngine::Shader
{
    namespace
    {
        constexpr uint64_t FnvOffset = 14695981039346656037ull;
        constexpr uint64_t FnvPrime = 1099511628211ull;

        template <typename T, bool IsEnum = std::is_enum_v<T>> struct HashRawType
        {
            using Type = T;
        };

        template <typename T> struct HashRawType<T, true>
        {
            using Type = std::underlying_type_t<T>;
        };

        /**
         * @brief 把任意基础字段追加到稳定 FNV-1a Hash。
         */
        template <typename T> void HashValue(uint64_t& hash, const T& value)
        {
            using Raw = typename HashRawType<T>::Type;
            using Unsigned = std::make_unsigned_t<Raw>;
            const Unsigned encoded = static_cast<Unsigned>(value);
            for (size_t i = 0; i < sizeof(Unsigned); ++i)
            {
                hash ^= static_cast<uint8_t>(encoded >> (i * 8));
                hash *= FnvPrime;
            }
        }

        /**
         * @brief 把字符串长度和内容追加到稳定 Hash，避免字段边界歧义。
         */
        void HashString(uint64_t& hash, std::string_view value)
        {
            HashValue(hash, static_cast<uint64_t>(value.size()));
            for (const char character : value)
            {
                hash ^= static_cast<uint8_t>(character);
                hash *= FnvPrime;
            }
        }

        /**
         * @brief 追加 Buffer Layout 的所有布局字段。
         */
        void HashBufferLayout(uint64_t& hash, const ShaderBufferLayout& layout)
        {
            HashString(hash, layout.name);
            HashValue(hash, layout.size);
            HashValue(hash, static_cast<uint64_t>(layout.members.size()));
            for (const auto& member : layout.members)
            {
                HashString(hash, member.name);
                HashValue(hash, member.type);
                HashValue(hash, member.offset);
                HashValue(hash, member.size);
                HashValue(hash, member.arrayCount);
                HashValue(hash, member.arrayStride);
                HashValue(hash, member.matrixStride);
            }
        }

        /**
         * @brief 对 Buffer 成员排序，使反射结果不依赖工具返回顺序。
         */
        void NormalizeBufferLayout(ShaderBufferLayout& layout)
        {
            std::ranges::sort(layout.members, {}, [](const ShaderBufferMember& member) { return std::tuple(member.offset, member.name); });
        }

        /**
         * @brief 返回用于诊断的 set/binding 文本。
         */
        std::string BindingLocation(uint32_t set, uint32_t binding)
        {
            return "set " + std::to_string(set) + ", binding " + std::to_string(binding);
        }
    } // namespace

    ShaderStageMask operator|(ShaderStageMask lhs, ShaderStageMask rhs)
    {
        return static_cast<ShaderStageMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    ShaderStageMask& operator|=(ShaderStageMask& lhs, ShaderStageMask rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    bool HasStage(ShaderStageMask mask, ShaderStageMask stage)
    {
        return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(stage)) != 0;
    }

    const ShaderResourceBinding* ShaderProgramInterface::FindResource(std::string_view name) const
    {
        const auto it = std::ranges::find(resources, name, &ShaderResourceBinding::name);
        return it == resources.end() ? nullptr : &*it;
    }

    const ShaderBufferMember* ShaderProgramInterface::FindBufferMember(std::string_view resourceName, std::string_view memberName) const
    {
        const ShaderResourceBinding* resource = FindResource(resourceName);
        if (!resource)
            return nullptr;
        const auto it = std::ranges::find(resource->buffer.members, memberName, &ShaderBufferMember::name);
        return it == resource->buffer.members.end() ? nullptr : &*it;
    }

    void NormalizeShaderReflection(ShaderReflectionData& reflection)
    {
        for (auto& resource : reflection.resources)
            NormalizeBufferLayout(resource.buffer);
        for (auto& range : reflection.pushConstants)
            NormalizeBufferLayout(range.buffer);

        std::ranges::sort(reflection.resources, {}, [](const ShaderResourceBinding& resource) { return std::tuple(resource.set, resource.binding, resource.name); });
        std::ranges::sort(reflection.pushConstants, {}, [](const ShaderPushConstantRange& range) { return std::tuple(range.offset, range.size, range.name); });
        std::ranges::sort(reflection.inputs, {}, [](const ShaderVertexInput& input) { return std::tuple(input.location, input.name); });
        std::ranges::sort(reflection.outputs, {}, [](const ShaderFragmentOutput& output) { return std::tuple(output.location, output.name); });
    }

    uint64_t HashShaderReflection(const ShaderReflectionData& reflection)
    {
        ShaderReflectionData normalized = reflection;
        NormalizeShaderReflection(normalized);

        uint64_t hash = FnvOffset;
        HashValue(hash, normalized.stage);
        HashString(hash, normalized.entryPoint);
        HashValue(hash, static_cast<uint64_t>(normalized.resources.size()));
        for (const auto& resource : normalized.resources)
        {
            HashString(hash, resource.name);
            HashValue(hash, resource.set);
            HashValue(hash, resource.binding);
            HashValue(hash, resource.type);
            HashValue(hash, resource.arrayCount);
            HashValue(hash, resource.stages);
            HashBufferLayout(hash, resource.buffer);
        }
        HashValue(hash, static_cast<uint64_t>(normalized.pushConstants.size()));
        for (const auto& range : normalized.pushConstants)
        {
            HashString(hash, range.name);
            HashValue(hash, range.offset);
            HashValue(hash, range.size);
            HashValue(hash, range.stages);
            HashBufferLayout(hash, range.buffer);
        }
        HashValue(hash, static_cast<uint64_t>(normalized.inputs.size()));
        for (const auto& input : normalized.inputs)
        {
            HashString(hash, input.name);
            HashValue(hash, input.location);
            HashValue(hash, input.type);
        }
        HashValue(hash, static_cast<uint64_t>(normalized.outputs.size()));
        for (const auto& output : normalized.outputs)
        {
            HashString(hash, output.name);
            HashValue(hash, output.location);
            HashValue(hash, output.type);
        }
        return hash;
    }

    uint64_t HashShaderProgramInterface(const ShaderProgramInterface& interface)
    {
        ShaderReflectionData combined{
            .stage = ShaderStageMask::None,
            .entryPoint = "program",
            .resources = interface.resources,
            .pushConstants = interface.pushConstants,
            .inputs = interface.vertexInputs,
            .outputs = interface.fragmentOutputs,
        };
        return HashShaderReflection(combined);
    }

    ShaderProgramBuildResult BuildShaderProgramInterface(std::span<const ShaderReflectionData> stages)
    {
        ShaderProgramBuildResult result;
        std::map<std::pair<uint32_t, uint32_t>, ShaderResourceBinding> resources;
        std::map<std::pair<uint32_t, uint32_t>, ShaderPushConstantRange> pushConstants;
        const ShaderReflectionData* vertex = nullptr;
        const ShaderReflectionData* fragment = nullptr;

        for (const ShaderReflectionData& source : stages)
        {
            ShaderReflectionData stage = source;
            NormalizeShaderReflection(stage);
            if (HasStage(stage.stage, ShaderStageMask::Vertex))
                vertex = &source;
            if (HasStage(stage.stage, ShaderStageMask::Fragment))
                fragment = &source;

            for (const auto& binding : stage.resources)
            {
                const auto key = std::pair(binding.set, binding.binding);
                const auto existing = resources.find(key);
                if (existing == resources.end())
                {
                    resources.emplace(key, binding);
                    continue;
                }
                if (existing->second.type != binding.type || existing->second.arrayCount != binding.arrayCount || existing->second.buffer != binding.buffer)
                {
                    result.errors.push_back("Descriptor conflict at " + BindingLocation(binding.set, binding.binding) + " between '" + existing->second.name + "' and '" + binding.name + "'");
                    continue;
                }
                if (existing->second.name != binding.name)
                    result.errors.push_back("Descriptor name conflict at " + BindingLocation(binding.set, binding.binding) + ": '" + existing->second.name + "' vs '" + binding.name + "'");
                existing->second.stages |= binding.stages;
            }

            for (const auto& range : stage.pushConstants)
            {
                const auto key = std::pair(range.offset, range.size);
                const auto existing = pushConstants.find(key);
                if (existing == pushConstants.end())
                {
                    pushConstants.emplace(key, range);
                    continue;
                }
                if (existing->second.buffer != range.buffer)
                {
                    result.errors.push_back("Push constant layout conflict at offset " + std::to_string(range.offset));
                    continue;
                }
                existing->second.stages |= range.stages;
            }
        }

        if (vertex && fragment)
        {
            for (const auto& input : fragment->inputs)
            {
                const auto output = std::ranges::find(vertex->outputs, input.location, &ShaderFragmentOutput::location);
                if (output == vertex->outputs.end())
                    result.errors.push_back("Fragment input location " + std::to_string(input.location) + " has no matching vertex output");
                else if (output->type != input.type)
                    result.errors.push_back("Stage interface type conflict at location " + std::to_string(input.location));
            }
        }

        for (auto& [key, binding] : resources)
            result.interface.resources.push_back(std::move(binding));
        for (auto& [key, range] : pushConstants)
            result.interface.pushConstants.push_back(std::move(range));
        if (vertex)
            result.interface.vertexInputs = vertex->inputs;
        if (fragment)
            result.interface.fragmentOutputs = fragment->outputs;

        ShaderReflectionData normalizedProgram{
            .resources = result.interface.resources,
            .pushConstants = result.interface.pushConstants,
            .inputs = result.interface.vertexInputs,
            .outputs = result.interface.fragmentOutputs,
        };
        NormalizeShaderReflection(normalizedProgram);
        result.interface.resources = std::move(normalizedProgram.resources);
        result.interface.pushConstants = std::move(normalizedProgram.pushConstants);
        result.interface.vertexInputs = std::move(normalizedProgram.inputs);
        result.interface.fragmentOutputs = std::move(normalizedProgram.outputs);
        result.interface.stableHash = HashShaderProgramInterface(result.interface);
        result.success = result.errors.empty();
        return result;
    }
} // namespace ChikaEngine::Shader
