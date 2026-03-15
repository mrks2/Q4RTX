// Vertex/index extraction from GL client arrays or VBOs.
//
// If a VBO is bound, the "pointer" from glVertexPointer etc is actually a byte
// offset into our CPU-side mirror. Wrapped in SEH because the game can pass
// stale pointers and we can't let that crash Q4.

#include "vertex_extract.h"
#include "vbo_tracker.h"
#include <gl/GL.h>
#include <cstring>

extern VBOTracker g_vboTracker;

static const uint8_t* ResolveAttribPtr(const void* ptr, uint32_t* out_buf_size = nullptr) {
    if (g_vboTracker.HasVBO()) {
        const uint8_t* vbo_data = g_vboTracker.GetBufferData(GL_ARRAY_BUFFER_ARB);
        if (!vbo_data) return nullptr;
        uint32_t total = g_vboTracker.GetBufferSize(GL_ARRAY_BUFFER_ARB);
        size_t offset = (size_t)ptr;
        if (offset >= total) return nullptr;
        if (out_buf_size) *out_buf_size = total - (uint32_t)offset;
        return vbo_data + offset;
    }
    return (const uint8_t*)ptr;
}

static void ExtractIndexed(GLsizei count, GLenum idx_type, const void* idx_ptr,
                           const TrackedGLState& st,
                           std::vector<SharedVertex>& out_verts,
                           std::vector<uint32_t>& out_idx)
{
    if (!st.vert_array_on) return;

    const uint8_t* index_data = nullptr;
    if (g_vboTracker.HasEBO()) {
        const uint8_t* ebo_data = g_vboTracker.GetBufferData(GL_ELEMENT_ARRAY_BUFFER_ARB);
        if (!ebo_data) return;
        index_data = ebo_data + (size_t)idx_ptr;
    } else {
        index_data = (const uint8_t*)idx_ptr;
        if (!index_data) return;
    }

    uint32_t vbo_size = 0;
    const uint8_t* vert_data = ResolveAttribPtr(st.vert_ptr, &vbo_size);
    if (!vert_data) return;

    const uint8_t* norm_data = (st.norm_array_on && st.norm_ptr)
                                ? ResolveAttribPtr(st.norm_ptr) : nullptr;

    out_idx.resize(count);
    uint32_t max_idx = 0;
    for (GLsizei i = 0; i < count; i++) {
        uint32_t idx = 0;
        switch (idx_type) {
            case GL_UNSIGNED_BYTE:  idx = index_data[i]; break;
            case GL_UNSIGNED_SHORT: idx = ((const uint16_t*)index_data)[i]; break;
            case GL_UNSIGNED_INT:   idx = ((const uint32_t*)index_data)[i]; break;
        }
        out_idx[i] = idx;
        if (idx > max_idx) max_idx = idx;
    }

    uint32_t nv = max_idx + 1;
    int v_stride = st.vert_stride ? st.vert_stride : (st.vert_size * (int)sizeof(float));
    int n_stride = st.norm_stride ? st.norm_stride : (3 * (int)sizeof(float));

    if (v_stride <= 0) return;
    if (vbo_size > 0 && (size_t)nv * v_stride > vbo_size) {
        nv = (uint32_t)(vbo_size / v_stride);
        if (nv == 0) return;
    }

    out_verts.resize(nv);
    for (uint32_t i = 0; i < nv; i++) {
        SharedVertex& v = out_verts[i];

        if (st.vert_type == GL_FLOAT && st.vert_size >= 3) {
            const float* fp = (const float*)(vert_data + (size_t)i * v_stride);
            v.px = fp[0]; v.py = fp[1]; v.pz = fp[2];
        }

        if (norm_data && st.norm_type == GL_FLOAT) {
            const float* fn = (const float*)(norm_data + (size_t)i * n_stride);
            v.nx = fn[0]; v.ny = fn[1]; v.nz = fn[2];
        } else {
            v.nx = 0.0f; v.ny = 1.0f; v.nz = 0.0f;
        }
    }
}

static void ExtractArrays(GLint first, GLsizei count, const TrackedGLState& st,
                           std::vector<SharedVertex>& out_verts,
                           std::vector<uint32_t>& out_idx)
{
    if (!st.vert_array_on) return;

    const uint8_t* vert_data = ResolveAttribPtr(st.vert_ptr);
    if (!vert_data) return;

    const uint8_t* norm_data = (st.norm_array_on && st.norm_ptr)
                                ? ResolveAttribPtr(st.norm_ptr) : nullptr;

    out_verts.resize(count);
    out_idx.resize(count);

    int v_stride = st.vert_stride ? st.vert_stride : (st.vert_size * (int)sizeof(float));
    int n_stride = st.norm_stride ? st.norm_stride : (3 * (int)sizeof(float));

    for (GLsizei i = 0; i < count; i++) {
        SharedVertex& v = out_verts[i];
        int vi = first + i;

        if (st.vert_type == GL_FLOAT && st.vert_size >= 3) {
            const float* fp = (const float*)(vert_data + (size_t)vi * v_stride);
            v.px = fp[0]; v.py = fp[1]; v.pz = fp[2];
        }

        if (norm_data && st.norm_type == GL_FLOAT) {
            const float* fn = (const float*)(norm_data + (size_t)vi * n_stride);
            v.nx = fn[0]; v.ny = fn[1]; v.nz = fn[2];
        } else {
            v.nx = 0.0f; v.ny = 1.0f; v.nz = 0.0f;
        }

        out_idx[i] = (uint32_t)i;
    }
}

// SEH wrappers

bool SafeExtractIndexed(GLsizei count, GLenum idx_type, const void* idx_ptr,
                        const TrackedGLState& st,
                        std::vector<SharedVertex>& out_verts,
                        std::vector<uint32_t>& out_idx) {
    __try {
        ExtractIndexed(count, idx_type, idx_ptr, st, out_verts, out_idx);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        out_verts.clear();
        out_idx.clear();
        return false;
    }
}

bool SafeExtractArrays(GLint first, GLsizei count, const TrackedGLState& st,
                        std::vector<SharedVertex>& out_verts,
                        std::vector<uint32_t>& out_idx) {
    __try {
        ExtractArrays(first, count, st, out_verts, out_idx);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        out_verts.clear();
        out_idx.clear();
        return false;
    }
}
