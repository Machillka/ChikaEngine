#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace ChikaEngine::Jobs
{
    /** @brief Merges per-chunk vectors by chunk index, independent of execution completion order. */
    template <typename Value> std::vector<Value> MergeDeterministicChunks(std::vector<std::vector<Value>>& chunks)
    {
        size_t totalSize = 0;
        for (const auto& chunk : chunks)
            totalSize += chunk.size();
        std::vector<Value> result;
        result.reserve(totalSize);
        for (auto& chunk : chunks)
        {
            for (auto& value : chunk)
                result.push_back(std::move(value));
        }
        return result;
    }
} // namespace ChikaEngine::Jobs
