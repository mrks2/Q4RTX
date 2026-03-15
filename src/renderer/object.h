#pragma once
#include "shared/shared_memory.h"
#include <vector>
#include <cstdint>

class Object {
public:
    Object() = default;

    // globalVertexOffset is subtracted from indices to make them local
    void SetGeometry(const SharedVertex* vertices, uint32_t vertexCount,
                     const uint32_t* indices, uint32_t indexCount,
                     uint32_t globalVertexOffset);

    const SharedVertex* GetVertices() const { return m_vertices.data(); }
    const uint32_t*     GetIndices()  const { return m_indices.data(); }
    uint32_t GetVertexCount() const { return (uint32_t)m_vertices.size(); }
    uint32_t GetIndexCount()  const { return (uint32_t)m_indices.size(); }

    const float* GetBoundsMin() const { return m_boundsMin; }
    const float* GetBoundsMax() const { return m_boundsMax; }

private:
    std::vector<SharedVertex> m_vertices;
    std::vector<uint32_t>     m_indices;
    float m_boundsMin[3] = {};
    float m_boundsMax[3] = {};

    void ComputeBounds();
};
