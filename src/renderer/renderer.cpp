#include "renderer/renderer.h"
#include "renderer/dxr_helpers.h"
#include <cstdio>

bool Renderer::Init(HWND hwnd, uint32_t width, uint32_t height) {
    if (!m_context.Init(hwnd, width, height)) return false;

    auto* device = m_context.GetDevice();

    // Heap layout: [0]=UAV, [1]=TLAS, [2]=verts, [3]=idx, [4]=lights
    D3D12_DESCRIPTOR_HEAP_DESC srvhd = {};
    srvhd.NumDescriptors = 5;
    srvhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device->CreateDescriptorHeap(&srvhd, IID_PPV_ARGS(&m_srvUavHeap));

    if (!m_pipeline.Init(device, width, height)) return false;

    m_pipeline.WriteOutputUAV(device, m_srvUavHeap->GetCPUDescriptorHandleForHeapStart());
    m_pipeline.BuildShaderTable(device);
    m_shaderTableBuilt = true;

    return true;
}

void Renderer::Shutdown() {
    m_context.Shutdown();
}

void Renderer::UpdateScene(const SharedFrameHeader& frame,
                           const SharedMeshHeader* meshHeaders,
                           const SharedVertex* vertices,
                           const uint32_t* indices,
                           const SharedLight* lights) {
    m_scene.Update(m_context, frame, meshHeaders, vertices, indices, lights);

    if (m_scene.IsReady()) {
        m_scene.WriteDescriptors(m_context.GetDevice(), m_srvUavHeap.Get(), 1);
    }
}

void Renderer::RenderFrame() {
    HRESULT hrDev = m_context.GetDevice()->GetDeviceRemovedReason();
    if (FAILED(hrDev)) { printf("ERROR: Device removed! hr=0x%08X\n", (unsigned)hrDev); return; }

    m_context.BeginFrame();

    if (m_scene.IsReady() && m_shaderTableBuilt) {
        DispatchRays();
    }

    CopyOutputToBackBuffer();
    m_context.Present();
}

void Renderer::DispatchRays() {
    auto* cmdList = m_context.GetCommandList();

    ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetComputeRootSignature(m_pipeline.GetRootSignature());
    cmdList->SetComputeRootDescriptorTable(0, m_srvUavHeap->GetGPUDescriptorHandleForHeapStart());

    if (m_scene.GetCameraCB())
        cmdList->SetComputeRootConstantBufferView(1, m_scene.GetCameraCB()->GetGPUVirtualAddress());

    if (m_scene.GetSkybox().GetConstantBuffer())
        cmdList->SetComputeRootConstantBufferView(2, m_scene.GetSkybox().GetConstantBuffer()->GetGPUVirtualAddress());

    cmdList->SetPipelineState1(m_pipeline.GetPipelineState());

    uint32_t recordSize = m_pipeline.GetRecordSize();
    auto tableAddr = m_pipeline.GetShaderTable()->GetGPUVirtualAddress();

    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = tableAddr;
    desc.RayGenerationShaderRecord.SizeInBytes = recordSize;
    desc.MissShaderTable.StartAddress = tableAddr + recordSize;
    desc.MissShaderTable.SizeInBytes = recordSize;
    desc.MissShaderTable.StrideInBytes = recordSize;
    desc.HitGroupTable.StartAddress = tableAddr + recordSize * 2;
    desc.HitGroupTable.SizeInBytes = recordSize;
    desc.HitGroupTable.StrideInBytes = recordSize;
    desc.Width = m_context.GetWidth();
    desc.Height = m_context.GetHeight();
    desc.Depth = 1;

    cmdList->DispatchRays(&desc);
}

void Renderer::CopyOutputToBackBuffer() {
    auto* cmdList = m_context.GetCommandList();
    auto* output = m_pipeline.GetOutputResource();
    auto* backBuffer = m_context.GetBackBuffer();

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = output;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = backBuffer;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    cmdList->ResourceBarrier(2, barriers);
    cmdList->CopyResource(backBuffer, output);

    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    cmdList->ResourceBarrier(2, barriers);
}
