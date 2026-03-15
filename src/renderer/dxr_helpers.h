#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>

using Microsoft::WRL::ComPtr;

static inline uint64_t Align(uint64_t val, uint64_t alignment) {
    return (val + alignment - 1) & ~(alignment - 1);
}

static inline ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* dev, uint64_t size,
                                                   D3D12_RESOURCE_FLAGS flags,
                                                   D3D12_RESOURCE_STATES state,
                                                   D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT) {
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = heap;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = size;
    rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags = flags;

    ComPtr<ID3D12Resource> res;
    dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, state, nullptr,
                                  IID_PPV_ARGS(&res));
    return res;
}

static inline ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* dev, const void* data, uint64_t size) {
    if (size == 0) size = 256;
    auto buf = CreateBuffer(dev, size, D3D12_RESOURCE_FLAG_NONE,
                            D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
    if (!buf) return nullptr;
    if (data) {
        void* mapped = nullptr;
        buf->Map(0, nullptr, &mapped);
        memcpy(mapped, data, size);
        buf->Unmap(0, nullptr);
    }
    return buf;
}

static inline std::vector<uint8_t> ReadFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read((char*)data.data(), size);
    return data;
}
