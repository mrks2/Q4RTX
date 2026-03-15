#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

class RTPipeline {
public:
    bool Init(ID3D12Device5* device, uint32_t width, uint32_t height);
    void BuildShaderTable(ID3D12Device5* device);
    void WriteOutputUAV(ID3D12Device5* device, D3D12_CPU_DESCRIPTOR_HANDLE destHandle);

    ID3D12StateObject*   GetPipelineState() { return m_pipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() { return m_rootSignature.Get(); }
    ID3D12Resource*      GetOutputResource() { return m_outputResource.Get(); }
    ID3D12Resource*      GetShaderTable()   { return m_shaderTable.Get(); }
    uint32_t             GetRecordSize() const { return m_recordSize; }

private:
    void CreateRootSignature(ID3D12Device5* device);
    void CreatePipelineState(ID3D12Device5* device);
    void CreateOutputResource(ID3D12Device5* device, uint32_t width, uint32_t height);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12StateObject>   m_pipelineState;
    ComPtr<ID3D12Resource>      m_outputResource;
    ComPtr<ID3D12Resource>      m_shaderTable;
    uint32_t                    m_recordSize = 0;
};
