#include "renderer/object.h"
#include <cfloat>

void Object::SetGeometry(const SharedVertex* vertices, uint32_t vertexCount,
                          const uint32_t* indices, uint32_t indexCount,
                          uint32_t globalVertexOffset) {
    m_vertices.assign(vertices, vertices + vertexCount);

    m_indices.resize(indexCount);
    for (uint32_t i = 0; i < indexCount; i++)
        m_indices[i] = indices[i] - globalVertexOffset;

    ComputeBounds();
}

void Object::ComputeBounds() {
    if (m_vertices.empty()) return;

    m_boundsMin[0] = m_boundsMin[1] = m_boundsMin[2] =  FLT_MAX;
    m_boundsMax[0] = m_boundsMax[1] = m_boundsMax[2] = -FLT_MAX;

    for (const auto& v : m_vertices) {
        if (v.px < m_boundsMin[0]) m_boundsMin[0] = v.px;
        if (v.py < m_boundsMin[1]) m_boundsMin[1] = v.py;
        if (v.pz < m_boundsMin[2]) m_boundsMin[2] = v.pz;
        if (v.px > m_boundsMax[0]) m_boundsMax[0] = v.px;
        if (v.py > m_boundsMax[1]) m_boundsMax[1] = v.py;
        if (v.pz > m_boundsMax[2]) m_boundsMax[2] = v.pz;
    }
}
