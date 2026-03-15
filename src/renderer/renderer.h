#pragma once
#include "renderer/d3d12_context.h"
#include "renderer/rt_pipeline.h"
#include "renderer/scene.h"
#include "renderer/frame_receiver.h"
#include <cstdint>

class Renderer {
public:
    bool Init(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    void UpdateScene(const SharedFrameHeader& frame,
                     const SharedMeshHeader* meshHeaders,
                     const SharedVertex* vertices,
                     const uint32_t* indices,
                     const SharedLight* lights);

    void RenderFrame();

private:
    void DispatchRays();
    void CopyOutputToBackBuffer();

    D3D12Context m_context;
    RTPipeline   m_pipeline;
    Scene        m_scene;

    // [0]=UAV, [1]=TLAS, [2]=verts, [3]=idx, [4]=lights
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

    bool m_shaderTableBuilt = false;
};
