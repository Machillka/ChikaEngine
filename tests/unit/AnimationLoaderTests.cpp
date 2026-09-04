#include "ChikaEngine/AnimationLoader.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace
{
    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    void WriteText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream file(path);
        file << text;
    }

    constexpr std::string_view kInterleavedAnimation = R"json({
  "asset": { "version": "2.0" },
  "buffers": [{
    "byteLength": 40,
    "uri": "data:application/octet-stream;base64,AAAAAABAmkQAAIA/AHCxRQAAAAAAAAAAAAAAAAAAgD8AAABAAABAQA=="
  }],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 16, "byteStride": 8 },
    { "buffer": 0, "byteOffset": 16, "byteLength": 24 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0.0], "max": [1.0] },
    { "bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC3" }
  ],
  "nodes": [{ "name": "InterleavedJoint" }],
  "animations": [{
    "name": "InterleavedTimes",
    "samplers": [{ "input": 0, "output": 1, "interpolation": "LINEAR" }],
    "channels": [{ "sampler": 0, "target": { "node": 0, "path": "translation" } }]
  }]
})json";

    constexpr std::string_view kInvalidNodeAnimation = R"json({
  "asset": { "version": "2.0" },
  "buffers": [{
    "byteLength": 40,
    "uri": "data:application/octet-stream;base64,AAAAAABAmkQAAIA/AHCxRQAAAAAAAAAAAAAAAAAAgD8AAABAAABAQA=="
  }],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 8 },
    { "buffer": 0, "byteOffset": 16, "byteLength": 24 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR" },
    { "bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC3" }
  ],
  "nodes": [{ "name": "OnlyNode" }],
  "animations": [{
    "samplers": [{ "input": 0, "output": 1, "interpolation": "LINEAR" }],
    "channels": [{ "sampler": 0, "target": { "node": 99, "path": "translation" } }]
  }]
})json";

    void TestAnimationLoader(const std::filesystem::path& directory)
    {
        const auto interleavedPath = directory / "interleaved.gltf";
        WriteText(interleavedPath, kInterleavedAnimation);
        const auto clip = ChikaEngine::Asset::AnimationLoader::Load(interleavedPath.string());
        Check(clip != nullptr, "valid interleaved animation loads");
        if (clip)
        {
            Check(clip->tracks.size() == 1, "valid animation creates one track");
            Check(std::abs(clip->duration - 1.0f) < 0.0001f, "interleaved input duration uses accessor stride");
            if (clip->tracks.size() == 1)
            {
                const auto& keys = clip->tracks[0].positionKeys;
                Check(keys.size() == 2, "valid animation creates two position keys");
                if (keys.size() == 2)
                    Check(std::abs(keys[1].time - 1.0f) < 0.0001f, "second interleaved key time is read from the next strided element");
            }
        }

        const auto invalidNodePath = directory / "invalid-node.gltf";
        WriteText(invalidNodePath, kInvalidNodeAnimation);
        Check(ChikaEngine::Asset::AnimationLoader::Load(invalidNodePath.string()) == nullptr, "out-of-range target node is rejected");
        Check(ChikaEngine::Asset::AnimationLoader::Load((directory / "missing.gltf").string()) == nullptr, "parse failure returns null");
    }
} // namespace

int main()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / ("chika_animation_loader_tests_" + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    TestAnimationLoader(directory);
    std::filesystem::remove_all(directory);

    if (g_failures != 0)
    {
        std::cerr << g_failures << " animation loader test(s) failed\n";
        return 1;
    }

    std::cout << "Animation loader tests passed\n";
    return 0;
}
