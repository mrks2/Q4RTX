#include "renderer/rt_pipeline.h"
#include "renderer/dxr_helpers.h"
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "d3d12.lib")

static const wchar_t* kRayGenName     = L"RayGen";
static const wchar_t* kMissName       = L"Miss";
static const wchar_t* kClosestHitName = L"ClosestHit";

bool RTPipeline::Init(ID3D12Device5* device, uint32_t width, uint32_t height) {
    CreateRootSignature(device);
    CreatePipelineState(device);
    CreateOutputResource(device, width, height);
    return m_pipelineState != nullptr;
}

// [0] = descriptor table (UAV u0 + SRV t0-t3)
// [1] = inline CBV b0 (camera)
// [2] = inline CBV b1 (skybox)
void RTPipeline::CreateRootSignature(ID3D12Device5* device) {
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[0].NumDescriptors = 1; ranges[0].BaseShaderRegister = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].NumDescriptors = 4; ranges[1].BaseShaderRegister = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = 1;

    D3D12_ROOT_PARAMETER params[3] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable.NumDescriptorRanges = 2;
    params[0].DescriptorTable.pDescriptorRanges = ranges;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].Descriptor.ShaderRegister = 0;
    params[1].Descriptor.RegisterSpace = 0;

    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[2].Descriptor.ShaderRegister = 1;
    params[2].Descriptor.RegisterSpace = 0;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 3;
    rsDesc.pParameters = params;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    if (errBlob) printf("Root sig error: %s\n", (char*)errBlob->GetBufferPointer());
    device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                IID_PPV_ARGS(&m_rootSignature));
}

void RTPipeline::CreatePipelineState(ID3D12Device5* device) {
    auto shaderBytes = ReadFileBytes("shaders/rt_shaders.cso");
    if (shaderBytes.empty()) {
        MessageBoxA(nullptr, "Failed to load shaders/rt_shaders.cso\nMake sure the shader was compiled with DXC.",
                    "Q4RTX Shader Error", MB_OK);
        return;
    }

    D3D12_STATE_SUBOBJECT subobjects[7] = {};
    uint32_t idx = 0;

    // DXIL library
    D3D12_DXIL_LIBRARY_DESC libDesc = {};
    libDesc.DXILLibrary.pShaderBytecode = shaderBytes.data();
    libDesc.DXILLibrary.BytecodeLength = shaderBytes.size();
    D3D12_EXPORT_DESC exports[3] = {};
    exports[0].Name = kRayGenName;
    exports[1].Name = kMissName;
    exports[2].Name = kClosestHitName;
    libDesc.NumExports = 3;
    libDesc.pExports = exports;
    subobjects[idx].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    subobjects[idx].pDesc = &libDesc;
    idx++;

    D3D12_HIT_GROUP_DESC hitGroup = {};
    hitGroup.HitGroupExport = L"HitGroup";
    hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.ClosestHitShaderImport = kClosestHitName;
    subobjects[idx].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[idx].pDesc = &hitGroup;
    idx++;

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = 4 * sizeof(float) + sizeof(uint32_t);
    shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(float);
    subobjects[idx].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subobjects[idx].pDesc = &shaderConfig;
    idx++;

    D3D12_GLOBAL_ROOT_SIGNATURE globalRSA = {};
    globalRSA.pGlobalRootSignature = m_rootSignature.Get();
    subobjects[idx].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subobjects[idx].pDesc = &globalRSA;
    idx++;

    // primary -> GI bounce -> shadow
    D3D12_RAYTRACING_PIPELINE_CONFIG pipeConfig = {};
    pipeConfig.MaxTraceRecursionDepth = 3;
    subobjects[idx].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[idx].pDesc = &pipeConfig;
    idx++;

    D3D12_STATE_OBJECT_DESC psoDesc = {};
    psoDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    psoDesc.NumSubobjects = idx;
    psoDesc.pSubobjects = subobjects;

    device->CreateStateObject(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
}

void RTPipeline::CreateOutputResource(ID3D12Device5* device, uint32_t width, uint32_t height) {
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width; texDesc.Height = height;
    texDesc.DepthOrArraySize = 1; texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &texDesc,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    nullptr, IID_PPV_ARGS(&m_outputResource));
}

void RTPipeline::WriteOutputUAV(ID3D12Device5* device, D3D12_CPU_DESCRIPTOR_HANDLE destHandle) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, destHandle);
}

void RTPipeline::BuildShaderTable(ID3D12Device5* device) {
    if (!m_pipelineState) return;

    ComPtr<ID3D12StateObjectProperties> props;
    m_pipelineState.As(&props);

    uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    m_recordSize = (uint32_t)Align(shaderIdSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    uint32_t tableSize = m_recordSize * 3;

    m_shaderTable = CreateBuffer(device, tableSize, D3D12_RESOURCE_FLAG_NONE,
                                 D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
    uint8_t* mapped = nullptr;
    m_shaderTable->Map(0, nullptr, (void**)&mapped);
    memset(mapped, 0, tableSize);
    memcpy(mapped + m_recordSize * 0, props->GetShaderIdentifier(L"RayGen"), shaderIdSize);
    memcpy(mapped + m_recordSize * 1, props->GetShaderIdentifier(L"Miss"), shaderIdSize);
    memcpy(mapped + m_recordSize * 2, props->GetShaderIdentifier(L"HitGroup"), shaderIdSize);
    m_shaderTable->Unmap(0, nullptr);

    printf("  Shader table: recordSize=%u tableSize=%u\n", m_recordSize, tableSize);
}
