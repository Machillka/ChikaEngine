#pragma once
#include <tiny_gltf.h>

namespace ChikaEngine::Asset
{

    template <typename T> static const T* GetGLTFDataPtr(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
    {
        if (accessor.bufferView < 0 || accessor.bufferView >= model.bufferViews.size())
            return nullptr;

        const auto& bufferView = model.bufferViews[accessor.bufferView];

        if (bufferView.buffer < 0 || bufferView.buffer >= model.buffers.size())
            return nullptr;

        const auto& buffer = model.buffers[bufferView.buffer];
        size_t offset = bufferView.byteOffset + accessor.byteOffset;

        if (buffer.data.empty() || offset >= buffer.data.size())
            return nullptr;

        return reinterpret_cast<const T*>(buffer.data.data() + offset);
    }

    template <typename T> static const T* GetElementPtr(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t index)
    {
        if (acc.bufferView < 0 || static_cast<size_t>(acc.bufferView) >= model.bufferViews.size())
            return nullptr;

        const auto& view = model.bufferViews[acc.bufferView];
        if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= model.buffers.size())
            return nullptr;

        const auto& buf = model.buffers[view.buffer];

        size_t compSize = tinygltf::GetComponentSizeInBytes(acc.componentType);
        size_t numComps = tinygltf::GetNumComponentsInType(acc.type);

        if (compSize == static_cast<size_t>(-1) || numComps == static_cast<size_t>(-1))
            return nullptr;

        size_t elementSize = compSize * numComps;
        size_t stride = view.byteStride == 0 ? elementSize : view.byteStride;
        size_t offset = view.byteOffset + acc.byteOffset + (index * stride);

        if (offset + elementSize > buf.data.size())
            return nullptr;

        return reinterpret_cast<const T*>(buf.data.data() + offset);
    }
} // namespace ChikaEngine::Asset