#include "ChikaEngine/AnimationLoader.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/GLTFHelper.hpp"
#include <tiny_gltf.h>
#include <algorithm>

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
            int nodeIdx = channel.target_node;
            const auto& node = model.nodes[nodeIdx];

            std::string targetJointName = node.name.empty() ? ("Joint_" + std::to_string(nodeIdx)) : node.name;

            AnimationTrack* track = nullptr;
            for (auto& t : clip->tracks)
            {
                if (t.jointName == targetJointName)
                {
                    track = &t;
                    break;
                }
            }
            if (!track)
            {
                clip->tracks.push_back({ targetJointName, {}, {}, {} });
                track = &clip->tracks.back();
            }

            const auto& sampler = gltfAnim.samplers[channel.sampler];
            const auto& inputAcc = model.accessors[sampler.input];
            const auto& outputAcc = model.accessors[sampler.output];

            const float* times = GetGLTFDataPtr<float>(model, inputAcc);
            const float* values = GetGLTFDataPtr<float>(model, outputAcc);

            if (inputAcc.count > 0)
            {
                clip->duration = std::max(clip->duration, times[inputAcc.count - 1]);
            }

            for (size_t i = 0; i < inputAcc.count; ++i)
            {
                float t = times[i];

                if (channel.target_path == "translation")
                {
                    Math::Vector3 pos(values[i * 3], values[i * 3 + 1], values[i * 3 + 2]);
                    track->positionKeys.push_back({ t, pos });
                }
                else if (channel.target_path == "rotation")
                {
                    // glTF 的四元数顺序是 X, Y, Z, W
                    Math::Quaternion rot(values[i * 4], values[i * 4 + 1], values[i * 4 + 2], values[i * 4 + 3]);
                    track->rotationKeys.push_back({ t, rot });
                }
                else if (channel.target_path == "scale")
                {
                    Math::Vector3 scale(values[i * 3], values[i * 3 + 1], values[i * 3 + 2]);
                    track->scaleKeys.push_back({ t, scale });
                }
            }
        }

        LOG_INFO("AnimationLoader", "Loaded Animation '{}' Duration: {}s, Tracks: {}", clip->name, clip->duration, clip->tracks.size());

        return clip;
    }
} // namespace ChikaEngine::Asset