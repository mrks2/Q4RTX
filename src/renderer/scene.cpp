#include "renderer/scene.h"
#include "renderer/d3d12_context.h"
#include "renderer/dxr_helpers.h"
#include <cstdio>
#include <cstring>
#include <unordered_map>

#pragma comment(lib, "d3d12.lib")

void Scene::Update(D3D12Context& ctx,
                   const SharedFrameHeader& frame,
                   const SharedMeshHeader* meshHeaders,
                   const SharedVertex* vertices,
                   const uint32_t* indices,
                   const SharedLight* lights) {
    if (frame.total_vertices == 0 || frame.total_indices == 0) return;

    ctx.WaitForGpu();

    ParseFrame(frame, meshHeaders, vertices, indices, lights);
    UploadToGPU(ctx.GetDevice());
    BuildAccelerationStructures(ctx);
    m_ready = true;
}

void Scene::ParseFrame(const SharedFrameHeader& frame,
                       const SharedMeshHeader* meshHeaders,
                       const SharedVertex* vertices,
                       const uint32_t* indices,
                       const SharedLight* lights) {
    memcpy(m_camera.pos, frame.cam_pos, sizeof(m_camera.pos));
    memcpy(m_camera.right, frame.cam_right, sizeof(m_camera.right));
    memcpy(m_camera.up, frame.cam_up, sizeof(m_camera.up));
    memcpy(m_camera.forward, frame.cam_forward, sizeof(m_camera.forward));
    m_camera.fovY = frame.cam_fov_y;
    m_camera.aspect = frame.cam_aspect;

    m_objects.clear();
    m_objects.reserve(frame.mesh_count);

    for (uint32_t i = 0; i < frame.mesh_count; i++) {
        const auto& mh = meshHeaders[i];
        uint32_t vStart = mh.vertex_offset / sizeof(SharedVertex);
        uint32_t iStart = mh.index_offset / sizeof(uint32_t);

        Object obj;
        obj.SetGeometry(&vertices[vStart], mh.vertex_count,
                        &indices[iStart], mh.index_count,
                        vStart);

        m_objects.push_back(std::move(obj));
    }

    uint32_t lightCount = frame.light_count < MAX_LIGHTS ? frame.light_count : MAX_LIGHTS;
    m_lights.clear();
    m_lights.reserve(lightCount);

    for (uint32_t i = 0; i < lightCount; i++) {
        const auto& sl = lights[i];
        m_lights.emplace_back(sl.pos[0], sl.pos[1], sl.pos[2],
                              sl.color[0], sl.color[1], sl.color[2],
                              sl.radius, sl.intensity);
    }

    printf("  Scene: %u objects, %u lights | Camera: pos=(%.1f,%.1f,%.1f) fov=%.1f\n",
           (uint32_t)m_objects.size(), (uint32_t)m_lights.size(),
           m_camera.pos[0], m_camera.pos[1], m_camera.pos[2],
           m_camera.fovY * 57.2958f);
}

void Scene::UploadToGPU(ID3D12Device5* device) {
    // Merge all objects into combined vertex/index arrays
    m_totalVertices = 0;
    m_totalIndices = 0;
    for (const auto& obj : m_objects) {
        m_totalVertices += obj.GetVertexCount();
        m_totalIndices += obj.GetIndexCount();
    }

    std::vector<SharedVertex> combinedVerts(m_totalVertices);
    std::vector<uint32_t> combinedIdx(m_totalIndices);

    uint32_t vOff = 0, iOff = 0;
    for (const auto& obj : m_objects) {
        memcpy(&combinedVerts[vOff], obj.GetVertices(),
               obj.GetVertexCount() * sizeof(SharedVertex));

        // Rebase local indices to global
        for (uint32_t i = 0; i < obj.GetIndexCount(); i++)
            combinedIdx[iOff + i] = obj.GetIndices()[i] + vOff;

        vOff += obj.GetVertexCount();
        iOff += obj.GetIndexCount();
    }

    m_vertexBuffer = CreateUploadBuffer(device, combinedVerts.data(),
                                         m_totalVertices * sizeof(SharedVertex));
    m_indexBuffer = CreateUploadBuffer(device, combinedIdx.data(),
                                        m_totalIndices * sizeof(uint32_t));

    if (!m_vertexBuffer || !m_indexBuffer) {
        printf("ERROR: Failed to create geometry buffers (verts=%u idx=%u)\n",
               m_totalVertices, m_totalIndices);
        return;
    }

    // Lights
    uint32_t lightCount = (uint32_t)m_lights.size();
    uint64_t lightBufSize = lightCount > 0 ? lightCount * sizeof(SharedLight) : sizeof(SharedLight);

    if (lightCount > 0) {
        std::vector<SharedLight> gpuLights(lightCount);
        for (uint32_t i = 0; i < lightCount; i++) {
            const auto& l = m_lights[i];
            gpuLights[i].pos[0] = l.GetPosition()[0];
            gpuLights[i].pos[1] = l.GetPosition()[1];
            gpuLights[i].pos[2] = l.GetPosition()[2];
            gpuLights[i].radius = l.GetRadius();
            gpuLights[i].color[0] = l.GetColor()[0];
            gpuLights[i].color[1] = l.GetColor()[1];
            gpuLights[i].color[2] = l.GetColor()[2];
            gpuLights[i].intensity = l.GetIntensity();
        }
        m_lightBuffer = CreateUploadBuffer(device, gpuLights.data(), lightBufSize);
    } else {
        m_lightBuffer = CreateUploadBuffer(device, nullptr, lightBufSize);
    }

    // Camera CB
    struct CameraConstants {
        float pos[4];
        float right[4];
        float up[4];
        float forward[4];
        float fovY;
        float aspect;
        uint32_t lightCount;
        float pad;
    };
    CameraConstants cam = {};
    cam.pos[0] = m_camera.pos[0]; cam.pos[1] = m_camera.pos[1]; cam.pos[2] = m_camera.pos[2]; cam.pos[3] = 1.0f;
    cam.right[0] = m_camera.right[0]; cam.right[1] = m_camera.right[1]; cam.right[2] = m_camera.right[2];
    cam.up[0] = m_camera.up[0]; cam.up[1] = m_camera.up[1]; cam.up[2] = m_camera.up[2];
    cam.forward[0] = m_camera.forward[0]; cam.forward[1] = m_camera.forward[1]; cam.forward[2] = m_camera.forward[2];
    cam.fovY = m_camera.fovY;
    cam.aspect = m_camera.aspect;
    cam.lightCount = lightCount;

    m_cameraCB = CreateUploadBuffer(device, &cam, Align(sizeof(cam), 256));

    m_skybox.Upload(device);
}

void Scene::BuildAccelerationStructures(D3D12Context& ctx) {
    auto* device = ctx.GetDevice();

    D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
    geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geomDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer->GetGPUVirtualAddress();
    geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(SharedVertex);
    geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geomDesc.Triangles.VertexCount = m_totalVertices;
    geomDesc.Triangles.IndexBuffer = m_indexBuffer->GetGPUVirtualAddress();
    geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geomDesc.Triangles.IndexCount = m_totalIndices;

    // BLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
    blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    blasInputs.NumDescs = 1;
    blasInputs.pGeometryDescs = &geomDesc;
    blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasInfo);

    m_blasScratch = CreateBuffer(device,
                                 blasInfo.ScratchDataSizeInBytes ? blasInfo.ScratchDataSizeInBytes : 256,
                                 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                 D3D12_RESOURCE_STATE_COMMON);
    m_blas = CreateBuffer(device,
                          blasInfo.ResultDataMaxSizeInBytes ? blasInfo.ResultDataMaxSizeInBytes : 256,
                          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
    if (!m_blasScratch || !m_blas) { printf("ERROR: Failed to create BLAS buffers\n"); return; }

    // TLAS - single instance, identity transform
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.Transform[0][0] = 1.0f;
    instanceDesc.Transform[1][1] = 1.0f;
    instanceDesc.Transform[2][2] = 1.0f;
    instanceDesc.InstanceMask = 0xFF;
    instanceDesc.AccelerationStructure = m_blas->GetGPUVirtualAddress();

    m_instanceBuffer = CreateUploadBuffer(device, &instanceDesc, sizeof(instanceDesc));

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlasInputs.NumDescs = 1;
    tlasInputs.InstanceDescs = m_instanceBuffer->GetGPUVirtualAddress();
    tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasInfo);

    m_tlasScratch = CreateBuffer(device,
                                 tlasInfo.ScratchDataSizeInBytes ? tlasInfo.ScratchDataSizeInBytes : 256,
                                 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                 D3D12_RESOURCE_STATE_COMMON);
    m_tlas = CreateBuffer(device,
                          tlasInfo.ResultDataMaxSizeInBytes ? tlasInfo.ResultDataMaxSizeInBytes : 256,
                          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                          D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
    if (!m_tlasScratch || !m_tlas || !m_instanceBuffer) {
        printf("ERROR: Failed to create TLAS buffers\n"); return;
    }

    printf("  BLAS: scratch=%llu result=%llu | TLAS: scratch=%llu result=%llu\n",
           blasInfo.ScratchDataSizeInBytes, blasInfo.ResultDataMaxSizeInBytes,
           tlasInfo.ScratchDataSizeInBytes, tlasInfo.ResultDataMaxSizeInBytes);

    HRESULT hrDev = device->GetDeviceRemovedReason();
    if (FAILED(hrDev)) { printf("ERROR: Device removed before AS build! hr=0x%08X\n", (unsigned)hrDev); return; }

    ctx.BeginFrame();
    auto* cmdList = ctx.GetCommandList();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc = {};
    blasBuildDesc.Inputs = blasInputs;
    blasBuildDesc.DestAccelerationStructureData = m_blas->GetGPUVirtualAddress();
    blasBuildDesc.ScratchAccelerationStructureData = m_blasScratch->GetGPUVirtualAddress();
    cmdList->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);

    // UAV barrier between BLAS and TLAS builds
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_blas.Get();
    cmdList->ResourceBarrier(1, &uavBarrier);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc = {};
    tlasBuildDesc.Inputs = tlasInputs;
    tlasBuildDesc.DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress();
    tlasBuildDesc.ScratchAccelerationStructureData = m_tlasScratch->GetGPUVirtualAddress();
    cmdList->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);

    ctx.ExecuteAndWait();

    hrDev = device->GetDeviceRemovedReason();
    if (FAILED(hrDev)) { printf("ERROR: Device removed after AS build! hr=0x%08X\n", (unsigned)hrDev); return; }
    printf("  AS build complete\n");
}

void Scene::WriteDescriptors(ID3D12Device5* device,
                             ID3D12DescriptorHeap* heap,
                             uint32_t startSlot) {
    UINT descIncr = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto heapStart = heap->GetCPUDescriptorHandleForHeapStart();

    // TLAS (t0)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = m_tlas->GetGPUVirtualAddress();
        auto handle = heapStart; handle.ptr += descIncr * startSlot;
        device->CreateShaderResourceView(nullptr, &srvDesc, handle);
    }

    // Vertices (t1)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.NumElements = m_totalVertices;
        srvDesc.Buffer.StructureByteStride = sizeof(SharedVertex);
        auto handle = heapStart; handle.ptr += descIncr * (startSlot + 1);
        device->CreateShaderResourceView(m_vertexBuffer.Get(), &srvDesc, handle);
    }

    // Indices (t2)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_UINT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.NumElements = m_totalIndices;
        auto handle = heapStart; handle.ptr += descIncr * (startSlot + 2);
        device->CreateShaderResourceView(m_indexBuffer.Get(), &srvDesc, handle);
    }

    // Lights (t3)
    if (m_lightBuffer) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.NumElements = max(1u, (uint32_t)(m_lightBuffer->GetDesc().Width / sizeof(SharedLight)));
        srvDesc.Buffer.StructureByteStride = sizeof(SharedLight);
        auto handle = heapStart; handle.ptr += descIncr * (startSlot + 3);
        device->CreateShaderResourceView(m_lightBuffer.Get(), &srvDesc, handle);
    }
}
