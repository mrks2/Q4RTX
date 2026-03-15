#include "renderer/d3d12_context.h"
#include <cstdio>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

bool D3D12Context::Init(HWND hwnd, uint32_t width, uint32_t height) {
    m_width = width; m_height = height;

#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
#endif

    CreateDXGIFactory1(IID_PPV_ARGS(&m_factory));

    // Find DXR-capable adapter
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1,
                                        IID_PPV_ARGS(&m_device)))) {
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
            m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5));
            if (opts5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0) break;
            m_device.Reset();
        }
        adapter.Reset();
    }

    if (!m_device) {
        MessageBoxA(nullptr, "No DXR-capable GPU found!", "Q4RTX Error", MB_OK);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    m_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_cmdQueue));

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = width; scd.Height = height;
    scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = FRAME_COUNT;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> sc1;
    m_factory->CreateSwapChainForHwnd(m_cmdQueue.Get(), hwnd, &scd, nullptr, nullptr, &sc1);
    sc1.As(&m_swapChain);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvhd = {};
    rtvhd.NumDescriptors = FRAME_COUNT;
    rtvhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    m_device->CreateDescriptorHeap(&rtvhd, IID_PPV_ARGS(&m_rtvHeap));
    m_rtvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FRAME_COUNT; i++) {
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescSize;
    }

    for (UINT i = 0; i < FRAME_COUNT; i++)
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc[i]));

    m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc[0].Get(),
                                nullptr, IID_PPV_ARGS(&m_cmdList));
    m_cmdList->Close();

    m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    return true;
}

void D3D12Context::Shutdown() {
    WaitForGpu();
    if (m_fenceEvent) CloseHandle(m_fenceEvent);
}

void D3D12Context::BeginFrame() {
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_cmdAlloc[m_frameIndex]->Reset();
    m_cmdList->Reset(m_cmdAlloc[m_frameIndex].Get(), nullptr);
}

void D3D12Context::ExecuteAndWait() {
    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_cmdQueue->ExecuteCommandLists(1, lists);
    WaitForGpu();
}

void D3D12Context::Present() {
    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_cmdQueue->ExecuteCommandLists(1, lists);
    m_swapChain->Present(1, 0);
    WaitForGpu();
}

void D3D12Context::WaitForGpu() {
    m_fenceValue++;
    m_cmdQueue->Signal(m_fence.Get(), m_fenceValue);
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}
