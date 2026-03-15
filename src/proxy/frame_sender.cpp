// Filters draw calls, transforms to world space, flushes buffered
// meshes + lights + camera to shared memory each frame.

#include "frame_sender.h"
#include "gl_proxy.h"
#include "vertex_extract.h"
#include "shared/shared_memory.h"
#include "shared/math_utils.h"
#include <vector>
#include <cstring>
#include <mutex>
#include <cstdio>

uint32_t g_capturedThisFrame = 0;

static float g_curViewMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static float g_invViewMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static bool  g_hasInvView = false;

static bool ShouldCapture(GLenum mode, GLsizei count, const TrackedGLState& st) {
    if (!st.depth_test)            return false;
    if (!st.depth_write)           return false;
    if (st.in_ortho)               return false;
    if (mode != GL_TRIANGLES)      return false;
    if (count < 6)                 return false;
    if (st.depth_range_near > 0.5) return false;
    return true;
}

struct CapturedMesh {
    std::vector<SharedVertex> vertices;
    std::vector<uint32_t>     indices;
};

struct CapturedLight {
    float pos[3];
    float color[3];
};

static std::vector<CapturedMesh>  g_meshes;
static std::vector<CapturedLight> g_lights;
static std::mutex                 g_mutex;

static HANDLE g_shm_handle = nullptr;
static void*  g_shm_ptr    = nullptr;
static HANDLE g_evt_ready   = nullptr;
static HANDLE g_evt_read    = nullptr;

// id Tech 4 multi-pass rendering can draw the same surface multiple times
// with depth_write=true. Skip duplicates by checking vertex count + first
// vertex position against already-captured meshes this frame.
static bool IsDuplicateMesh(const std::vector<SharedVertex>& verts,
                             const std::vector<uint32_t>& indices) {
    if (verts.empty()) return false;
    for (const auto& existing : g_meshes) {
        if (existing.vertices.size() != verts.size()) continue;
        if (existing.indices.size() != indices.size()) continue;
        const auto& a = existing.vertices[0];
        const auto& b = verts[0];
        float dx = a.px - b.px, dy = a.py - b.py, dz = a.pz - b.pz;
        if (dx*dx + dy*dy + dz*dz < 0.01f) return true;
    }
    return false;
}

static bool IsSkyboxMesh(const std::vector<SharedVertex>& verts) {
    if (verts.size() < 3) return false;
    float minX = verts[0].px, maxX = minX;
    float minY = verts[0].py, maxY = minY;
    float minZ = verts[0].pz, maxZ = minZ;
    for (const auto& v : verts) {
        if (v.px < minX) minX = v.px; if (v.px > maxX) maxX = v.px;
        if (v.py < minY) minY = v.py; if (v.py > maxY) maxY = v.py;
        if (v.pz < minZ) minZ = v.pz; if (v.pz > maxZ) maxZ = v.pz;
    }
    float extX = maxX - minX, extY = maxY - minY, extZ = maxZ - minZ;
    float maxExtent = extX > extY ? extX : extY;
    if (extZ > maxExtent) maxExtent = extZ;

    if (maxExtent > 4000.0f && verts.size() <= 36) return true;
    if (maxExtent > 16000.0f) return true;
    return false;
}

static void TransformToWorldSpace(std::vector<SharedVertex>& verts, const float* modelview) {
    if (!g_hasInvView) return;

    float model[16];
    math::MatMul4x4(g_invViewMatrix, modelview, model);
    if (math::IsIdentity(model)) return;

    for (auto& v : verts) {
        math::TransformPos(model, v.px, v.py, v.pz);
        math::TransformNorm(model, v.nx, v.ny, v.nz);
    }
}

void frame_sender::SetViewMatrix(const float* view_mat) {
    memcpy(g_curViewMatrix, view_mat, 64);
    g_hasInvView = math::InvertMatrix(view_mat, g_invViewMatrix);
}

bool frame_sender::Init() {
    g_shm_handle = OpenOrCreateSharedMem(&g_shm_ptr, true, SHM_NAME, SHM_SIZE);
    if (!g_shm_handle) return false;
    memset(g_shm_ptr, 0, SHM_SIZE);

    g_evt_ready = CreateEventA(nullptr, FALSE, FALSE, EVT_DATA_READY);
    g_evt_read  = CreateEventA(nullptr, FALSE, TRUE,  EVT_DATA_READ);
    return g_evt_ready && g_evt_read;
}

void frame_sender::Shutdown() {
    if (g_shm_ptr)    { UnmapViewOfFile(g_shm_ptr); g_shm_ptr = nullptr; }
    if (g_shm_handle) { CloseHandle(g_shm_handle); g_shm_handle = nullptr; }
    if (g_evt_ready)  { CloseHandle(g_evt_ready); g_evt_ready = nullptr; }
    if (g_evt_read)   { CloseHandle(g_evt_read); g_evt_read = nullptr; }
}

void frame_sender::AddLight(const float* light_origin_local, const float* light_color,
                             const float* modelview) {
    if (!g_hasInvView) return;

    float model[16];
    math::MatMul4x4(g_invViewMatrix, modelview, model);

    float wx = model[0]*light_origin_local[0] + model[4]*light_origin_local[1] + model[8]*light_origin_local[2]  + model[12];
    float wy = model[1]*light_origin_local[0] + model[5]*light_origin_local[1] + model[9]*light_origin_local[2]  + model[13];
    float wz = model[2]*light_origin_local[0] + model[6]*light_origin_local[1] + model[10]*light_origin_local[2] + model[14];

    // dedupe lights within 2 units
    for (const auto& existing : g_lights) {
        float dx = existing.pos[0] - wx, dy = existing.pos[1] - wy, dz = existing.pos[2] - wz;
        if (dx*dx + dy*dy + dz*dz < 4.0f) return;
    }

    if (g_lights.size() >= MAX_LIGHTS) return;

    CapturedLight light;
    light.pos[0] = wx; light.pos[1] = wy; light.pos[2] = wz;
    light.color[0] = light_color[0]; light.color[1] = light_color[1]; light.color[2] = light_color[2];

    std::lock_guard<std::mutex> lock(g_mutex);
    g_lights.push_back(light);
}

void frame_sender::OnDrawElements(GLenum mode, GLsizei count, GLenum type,
                                    const void* indices, const TrackedGLState& st,
                                    const float* modelview) {
    if (!ShouldCapture(mode, count, st)) return;

    CapturedMesh mesh;
    if (!SafeExtractIndexed(count, type, indices, st, mesh.vertices, mesh.indices)) return;
    if (mesh.vertices.empty() || IsSkyboxMesh(mesh.vertices)) return;

    TransformToWorldSpace(mesh.vertices, modelview);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (IsDuplicateMesh(mesh.vertices, mesh.indices)) return;
    g_capturedThisFrame++;
    g_meshes.push_back(std::move(mesh));
}

void frame_sender::OnDrawArrays(GLenum mode, GLint first, GLsizei count,
                                  const TrackedGLState& st, const float* modelview) {
    if (!ShouldCapture(mode, count, st)) return;

    CapturedMesh mesh;
    if (!SafeExtractArrays(first, count, st, mesh.vertices, mesh.indices)) return;
    if (mesh.vertices.empty() || IsSkyboxMesh(mesh.vertices)) return;

    TransformToWorldSpace(mesh.vertices, modelview);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (IsDuplicateMesh(mesh.vertices, mesh.indices)) return;
    g_capturedThisFrame++;
    g_meshes.push_back(std::move(mesh));
}

void frame_sender::FlushFrame(uint32_t frame_number,
                                const float* view_mat, const float* proj_mat) {
    if (WaitForSingleObject(g_evt_read, 0) != WAIT_OBJECT_0) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_meshes.clear();
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    uint8_t* base = (uint8_t*)g_shm_ptr;
    auto* frame = (SharedFrameHeader*)(base + OFFSET_FRAME_HEADER);
    auto* mhdrs = (SharedMeshHeader*)(base + OFFSET_MESH_HEADERS);
    auto* verts = (SharedVertex*)(base + OFFSET_VERTEX_DATA);
    auto* idxs  = (uint32_t*)(base + OFFSET_INDEX_DATA);

    frame->frame_number   = frame_number;
    frame->mesh_count     = 0;
    frame->total_vertices = 0;
    frame->total_indices  = 0;

    // camera from inverse view matrix
    float inv_view[16];
    if (math::InvertMatrix(view_mat, inv_view)) {
        frame->cam_pos[0] = inv_view[12]; frame->cam_pos[1] = inv_view[13]; frame->cam_pos[2] = inv_view[14];
        frame->cam_right[0]   = inv_view[0]; frame->cam_right[1]   = inv_view[1]; frame->cam_right[2]   = inv_view[2];
        frame->cam_up[0]      = inv_view[4]; frame->cam_up[1]      = inv_view[5]; frame->cam_up[2]      = inv_view[6];
        frame->cam_forward[0] = -inv_view[8]; frame->cam_forward[1] = -inv_view[9]; frame->cam_forward[2] = -inv_view[10];
    }

    if (fabsf(proj_mat[5]) > 0.001f) {
        frame->cam_fov_y = 2.0f * atanf(1.0f / proj_mat[5]);
        frame->cam_aspect = proj_mat[5] / proj_mat[0];
    } else {
        frame->cam_fov_y = 1.0472f;
        frame->cam_aspect = 4.0f / 3.0f;
    }
    float A = proj_mat[10], B = proj_mat[14];
    if (fabsf(A + 1.0f) > 0.001f) {
        frame->cam_near = B / (A - 1.0f);
        frame->cam_far  = B / (A + 1.0f);
    } else {
        frame->cam_near = 1.0f;
        frame->cam_far = 16384.0f;
    }

    uint32_t v_off = 0, i_off = 0, m_idx = 0;
    for (auto& m : g_meshes) {
        if (m_idx >= MAX_MESHES) break;
        if (v_off + m.vertices.size() > MAX_VERTICES) break;
        if (i_off + m.indices.size() > MAX_INDICES) break;

        auto& h = mhdrs[m_idx];
        h.vertex_count  = (uint32_t)m.vertices.size();
        h.index_count   = (uint32_t)m.indices.size();
        h.vertex_offset = v_off * sizeof(SharedVertex);
        h.index_offset  = i_off * sizeof(uint32_t);
        h.flags         = 0;

        memcpy(&verts[v_off], m.vertices.data(), m.vertices.size() * sizeof(SharedVertex));
        for (size_t i = 0; i < m.indices.size(); i++)
            idxs[i_off + i] = m.indices[i] + v_off;

        v_off += (uint32_t)m.vertices.size();
        i_off += (uint32_t)m.indices.size();
        m_idx++;
    }

    frame->mesh_count     = m_idx;
    frame->total_vertices = v_off;
    frame->total_indices  = i_off;

    auto* lights = (SharedLight*)(base + OFFSET_LIGHT_DATA);
    uint32_t l_count = 0;
    for (const auto& l : g_lights) {
        if (l_count >= MAX_LIGHTS) break;
        lights[l_count].pos[0] = l.pos[0]; lights[l_count].pos[1] = l.pos[1]; lights[l_count].pos[2] = l.pos[2];
        lights[l_count].radius = 500.0f;
        lights[l_count].color[0] = l.color[0]; lights[l_count].color[1] = l.color[1]; lights[l_count].color[2] = l.color[2];
        lights[l_count].intensity = 1.0f;
        l_count++;
    }
    frame->light_count = l_count;

    g_meshes.clear();
    g_lights.clear();

    SetEvent(g_evt_ready);
}
