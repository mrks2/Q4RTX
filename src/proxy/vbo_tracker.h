#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

#define GL_ARRAY_BUFFER_ARB           0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB   0x8893
#define GL_STATIC_DRAW_ARB            0x88E4

// CPU-side mirror of VBO contents so we can read back geometry for capture.
class VBOTracker {
public:
    struct BufferData {
        std::vector<uint8_t> data;
        uint32_t size = 0;
    };

    void BindBuffer(uint32_t target, uint32_t buffer) {
        if (target == GL_ARRAY_BUFFER_ARB)
            bound_vbo = buffer;
        else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB)
            bound_ebo = buffer;
    }

    void BufferUpload(uint32_t target, int size, const void* data, uint32_t /*usage*/) {
        uint32_t id = (target == GL_ARRAY_BUFFER_ARB) ? bound_vbo : bound_ebo;
        if (id == 0) return;

        auto& buf = buffers[id];
        buf.size = size;
        buf.data.resize(size);
        if (data) {
            memcpy(buf.data.data(), data, size);
        }
    }

    void BufferSubData(uint32_t target, int offset, int size, const void* data) {
        uint32_t id = (target == GL_ARRAY_BUFFER_ARB) ? bound_vbo : bound_ebo;
        if (id == 0) return;

        auto it = buffers.find(id);
        if (it == buffers.end()) return;

        auto& buf = it->second;
        if (offset + size > (int)buf.data.size()) {
            buf.data.resize(offset + size);
            buf.size = offset + size;
        }
        memcpy(buf.data.data() + offset, data, size);
    }

    void DeleteBuffers(int n, const uint32_t* ids) {
        for (int i = 0; i < n; i++)
            buffers.erase(ids[i]);
    }

    const uint8_t* GetBufferData(uint32_t target) const {
        uint32_t id = (target == GL_ARRAY_BUFFER_ARB) ? bound_vbo : bound_ebo;
        if (id == 0) return nullptr;
        auto it = buffers.find(id);
        if (it == buffers.end()) return nullptr;
        return it->second.data.data();
    }

    uint32_t GetBufferSize(uint32_t target) const {
        uint32_t id = (target == GL_ARRAY_BUFFER_ARB) ? bound_vbo : bound_ebo;
        if (id == 0) return 0;
        auto it = buffers.find(id);
        if (it == buffers.end()) return 0;
        return it->second.size;
    }

    bool HasVBO() const { return bound_vbo != 0; }
    bool HasEBO() const { return bound_ebo != 0; }
    uint32_t GetBoundVBO() const { return bound_vbo; }
    uint32_t GetBoundEBO() const { return bound_ebo; }

private:
    uint32_t bound_vbo = 0;
    uint32_t bound_ebo = 0;
    std::unordered_map<uint32_t, BufferData> buffers;
};

extern VBOTracker g_vboTracker;
