#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

class D3D12Context {
public:
    static constexpr uint32_t FRAME_COUNT = 2;

    bool Init(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    void BeginFrame();
    void ExecuteAndWait();
    void Present();
    void WaitForGpu();

    ID3D12Device5*              GetDevice()       { return m_device.Get(); }
    ID3D12GraphicsCommandList4* GetCommandList()   { return m_cmdList.Get(); }
    ID3D12CommandQueue*         GetCommandQueue()  { return m_cmdQueue.Get(); }
    ID3D12Resource*             GetBackBuffer()    { return m_renderTargets[m_frameIndex].Get(); }
    uint32_t GetWidth()      const { return m_width; }
    uint32_t GetHeight()     const { return m_height; }
    uint32_t GetFrameIndex() const { return m_frameIndex; }

private:
    ComPtr<IDXGIFactory4>              m_factory;
    ComPtr<ID3D12Device5>              m_device;
    ComPtr<ID3D12CommandQueue>         m_cmdQueue;
    ComPtr<ID3D12CommandAllocator>     m_cmdAlloc[FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList4> m_cmdList;
    ComPtr<IDXGISwapChain3>            m_swapChain;

    ComPtr<ID3D12DescriptorHeap>       m_rtvHeap;
    ComPtr<ID3D12Resource>             m_renderTargets[FRAME_COUNT];
    uint32_t m_rtvDescSize = 0;

    ComPtr<ID3D12Fence>  m_fence;
    uint64_t             m_fenceValue = 0;
    HANDLE               m_fenceEvent = nullptr;

    uint32_t m_width = 0, m_height = 0;
    uint32_t m_frameIndex = 0;
};
