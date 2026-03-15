#pragma once
#include <cstdint>
#include <Windows.h>

// Shared memory layout between hook DLL and RT renderer.
// Hook writes captured geometry each frame, renderer reads it.

constexpr const char* SHM_NAME      = "Q4RTX_SharedGeometry";
constexpr const char* EVT_DATA_READY = "Q4RTX_DataReady";
constexpr const char* EVT_DATA_READ  = "Q4RTX_DataRead";

constexpr uint32_t SHM_SIZE         = 64 * 1024 * 1024;  // 64 MB
constexpr uint32_t MAX_MESHES       = 4096;
constexpr uint32_t MAX_VERTICES     = 500000;
constexpr uint32_t MAX_INDICES      = 1500000;
constexpr uint32_t MAX_LIGHTS       = 128;

#pragma pack(push, 1)

struct SharedVertex {
    float px, py, pz;
    float nx, ny, nz;
};

struct SharedMeshHeader {
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t vertex_offset;  // byte offset into vertex_data[]
    uint32_t index_offset;   // byte offset into index_data[]
    uint32_t flags;
};

struct SharedLight {
    float pos[3];
    float radius;
    float color[3];
    float intensity;
};

struct SharedFrameHeader {
    uint32_t frame_number;
    uint32_t mesh_count;
    uint32_t total_vertices;
    uint32_t total_indices;

    // Camera (extracted from GL matrices by proxy)
    float    cam_pos[3];
    float    cam_forward[3];
    float    cam_up[3];
    float    cam_right[3];
    float    cam_fov_y;       // vertical FOV in radians
    float    cam_aspect;
    float    cam_near;
    float    cam_far;

    uint32_t light_count;
};

// Layout:  FrameHeader | MeshHeaders[MAX] | Vertices[MAX] | Indices[MAX] | Lights[MAX]

constexpr uint32_t OFFSET_FRAME_HEADER = 0;
constexpr uint32_t OFFSET_MESH_HEADERS = sizeof(SharedFrameHeader);
constexpr uint32_t OFFSET_VERTEX_DATA  = OFFSET_MESH_HEADERS + sizeof(SharedMeshHeader) * MAX_MESHES;
constexpr uint32_t OFFSET_INDEX_DATA   = OFFSET_VERTEX_DATA + sizeof(SharedVertex) * MAX_VERTICES;
constexpr uint32_t OFFSET_LIGHT_DATA   = OFFSET_INDEX_DATA + sizeof(uint32_t) * MAX_INDICES;

#pragma pack(pop)

inline HANDLE OpenOrCreateSharedMem(void** out_ptr, bool create,
                                     const char* name = SHM_NAME,
                                     uint32_t size = SHM_SIZE) {
    HANDLE hMap;
    if (create) {
        hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  0, size, name);
    } else {
        hMap = OpenFileMappingA(FILE_MAP_READ, FALSE, name);
    }
    if (!hMap) { *out_ptr = nullptr; return nullptr; }

    DWORD access = create ? FILE_MAP_WRITE : FILE_MAP_READ;
    *out_ptr = MapViewOfFile(hMap, access, 0, 0, size);
    if (!*out_ptr) { CloseHandle(hMap); return nullptr; }
    return hMap;
}
