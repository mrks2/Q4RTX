#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>
#include "shared/shared_memory.h"
#include "renderer/object.h"
#include "renderer/light.h"
#include "renderer/skybox.h"

using Microsoft::WRL::ComPtr;

struct Camera {
    float pos[3]     = {};
    float right[3]   = {};
    float up[3]      = {};
    float forward[3] = {};
    float fovY       = 1.0f;
    float aspect     = 1.333f;
};

class D3D12Context;

class Scene {
public:
    void Update(D3D12Context& ctx,
                const SharedFrameHeader& frame,
                const SharedMeshHeader* meshHeaders,
                const SharedVertex* vertices,
                const uint32_t* indices,
                const SharedLight* lights);

    // Writes slots [startSlot..startSlot+3]: TLAS, verts, indices, lights
    void WriteDescriptors(ID3D12Device5* device,
                          ID3D12DescriptorHeap* heap,
                          uint32_t startSlot);

    bool IsReady() const { return m_ready; }
    const Camera& GetCamera() const { return m_camera; }
    ID3D12Resource* GetCameraCB() { return m_cameraCB.Get(); }
    Skybox& GetSkybox() { return m_skybox; }

    const std::vector<Object>& GetObjects() const { return m_objects; }
    const std::vector<Light>&  GetLights()  const { return m_lights; }
    uint32_t GetObjectCount() const { return (uint32_t)m_objects.size(); }
    uint32_t GetLightCount()  const { return (uint32_t)m_lights.size(); }

private:
    void ParseFrame(const SharedFrameHeader& frame,
                    const SharedMeshHeader* meshHeaders,
                    const SharedVertex* vertices,
                    const uint32_t* indices,
                    const SharedLight* lights);

    void UploadToGPU(ID3D12Device5* device);
    void BuildAccelerationStructures(D3D12Context& ctx);

    std::vector<Object> m_objects;
    std::vector<Light>  m_lights;
    Camera m_camera;
    Skybox m_skybox;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    uint32_t m_totalVertices = 0;
    uint32_t m_totalIndices  = 0;

    ComPtr<ID3D12Resource> m_blas;
    ComPtr<ID3D12Resource> m_blasScratch;
    ComPtr<ID3D12Resource> m_tlas;
    ComPtr<ID3D12Resource> m_tlasScratch;
    ComPtr<ID3D12Resource> m_instanceBuffer;

    ComPtr<ID3D12Resource> m_lightBuffer;
    ComPtr<ID3D12Resource> m_cameraCB;

    bool m_ready = false;
};
