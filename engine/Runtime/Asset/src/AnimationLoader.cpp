#include "ChikaEngine/AnimationLoader.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <tiny_gltf.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
    bool ReadFloatElement(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, size_t componentCount, std::array<float, 4>& values)
    {
        if (index >= accessor.count || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || static_cast<size_t>(tinygltf::GetNumComponentsInType(accessor.type)) != componentCount || accessor.sparse.isSparse || accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= model.bufferViews.size())
            return false;

        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= model.buffers.size())
            return false;

        const int byteStride = accessor.ByteStride(view);
        const size_t elementSize = sizeof(float) * componentCount;
        if (byteStride < 0 || static_cast<size_t>(byteStride) < elementSize || accessor.byteOffset > view.byteLength)
            return false;

        const size_t stride = static_cast<size_t>(byteStride);
        if (index > (std::numeric_limits<size_t>::max() - accessor.byteOffset) / stride)
            return false;
        const size_t relativeOffset = accessor.byteOffset + index * stride;
        if (relativeOffset > view.byteLength || elementSize > view.byteLength - relativeOffset)
            return false;

        const auto& buffer = model.buffers[view.buffer].data;
        if (view.byteOffset > buffer.size() || relativeOffset > buffer.size() - view.byteOffset)
            return false;
        const size_t absoluteOffset = view.byteOffset + relativeOffset;
        if (elementSize > buffer.size() - absoluteOffset)
            return false;

        std::memcpy(values.data(), buffer.data() + absoluteOffset, elementSize);
        return std::all_of(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(componentCount), [](float value) { return std::isfinite(value); });
    }
} // namespace

namespace ChikaEngine::Asset
{
    std::unique_ptr<AnimationClipData> AnimationLoader::Load(const std::string& path)
    {
        LOG_INFO("AnimationLoader", "Loading GLTF Animation: {}", path);

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        if (!ret)
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);

        if (!ret)
        {
            LOG_ERROR("AnimationLoader", "Failed to load GLTF: {} | Error: {}", path, err);
            return nullptr;
        }

        auto clip = std::make_unique<AnimationClipData>();
        clip->name = path;
        clip->duration = 0.0f;

        if (model.animations.empty())
        {
            LOG_WARN("AnimationLoader", "No animations found in file: {}", path);
            return clip;
        }

        // TODO: 目前做 demo 仅加载文件中的第一个 Animation
        const auto& gltfAnim = model.animations[0];
        clip->name = gltfAnim.name.empty() ? path : gltfAnim.name;

        for (const auto& channel : gltfAnim.channels)
        {
            const int nodeIdx = channel.target_node;
            if (nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= model.nodes.size() || channel.sampler < 0 || static_cast<size_t>(channel.sampler) >= gltfAnim.samplers.size())
            {
                LOG_ERROR("AnimationLoader", "Animation '{}' contains an invalid channel target or sampler index", clip->name);
                return nullptr;
            }

            size_t outputComponentCount = 0;
            if (channel.target_path == "translation" || channel.target_path == "scale")
                outputComponentCount = 3;
            else if (channel.target_path == "rotation")
                outputComponentCount = 4;
            else
            {
                LOG_ERROR("AnimationLoader", "Animation '{}' uses unsupported target path '{}'", clip->name, channel.target_path);
                return nullptr;
            }

            const auto& node = model.nodes[nodeIdx];
            std::string targetJointName = node.name.empty() ? ("Joint_" + std::to_string(nodeIdx)) : node.name;

            const auto& sampler = gltfAnim.samplers[channel.sampler];
            if (sampler.interpolation != "LINEAR" || sampler.input < 0 || static_cast<size_t>(sampler.input) >= model.accessors.size() || sampler.output < 0 || static_cast<size_t>(sampler.output) >= model.accessors.size())
            {
                LOG_ERROR("AnimationLoader", "Animation '{}' contains unsupported interpolation or invalid accessor indices", clip->name);
                return nullptr;
            }

            const auto& inputAcc = model.accessors[sampler.input];
            const auto& outputAcc = model.accessors[sampler.output];
            if (inputAcc.count == 0 || outputAcc.count != inputAcc.count)
            {
                LOG_ERROR("AnimationLoader", "Animation '{}' input/output key counts do not match", clip->name);
                return nullptr;
            }

            AnimationTrack* track = nullptr;
            for (auto& existingTrack : clip->tracks)
            {
                if (existingTrack.jointName == targetJointName)
                {
                    track = &existingTrack;
                    break;
                }
            }
            if (!track)
            {
                clip->tracks.push_back({ targetJointName, {}, {}, {} });
                track = &clip->tracks.back();
            }

            float previousTime = -std::numeric_limits<float>::infinity();
            for (size_t i = 0; i < inputAcc.count; ++i)
            {
                std::array<float, 4> timeValues{};
                std::array<float, 4> outputValues{};
                if (!ReadFloatElement(model, inputAcc, i, 1, timeValues) || !ReadFloatElement(model, outputAcc, i, outputComponentCount, outputValues) || timeValues[0] <= previousTime)
                {
                    LOG_ERROR("AnimationLoader", "Animation '{}' contains invalid accessor data", clip->name);
                    return nullptr;
                }
                const float time = timeValues[0];
                previousTime = time;

                if (channel.target_path == "translation")
                {
                    Math::Vector3 position(outputValues[0], outputValues[1], outputValues[2]);
                    track->positionKeys.push_back({ time, position });
                }
                else if (channel.target_path == "rotation")
                {
                    // glTF 的四元数顺序是 X, Y, Z, W
                    Math::Quaternion rotation(outputValues[0], outputValues[1], outputValues[2], outputValues[3]);
                    track->rotationKeys.push_back({ time, rotation });
                }
                else if (channel.target_path == "scale")
                {
                    Math::Vector3 scale(outputValues[0], outputValues[1], outputValues[2]);
                    track->scaleKeys.push_back({ time, scale });
                }
            }
            clip->duration = std::max(clip->duration, previousTime);
        }

        LOG_INFO("AnimationLoader", "Loaded Animation '{}' Duration: {}s, Tracks: {}", clip->name, clip->duration, clip->tracks.size());

        return clip;
    }
} // namespace ChikaEngine::Asset
