#pragma once

#include "gl_state.h"
#include "shared/shared_memory.h"
#include <vector>
#include <cstdint>

// SEH-wrapped geometry extraction from GL arrays/VBOs.
// Returns false if an access violation occurred (stale pointer from the game).

bool SafeExtractIndexed(GLsizei count, GLenum idx_type, const void* idx_ptr,
                        const TrackedGLState& st,
                        std::vector<SharedVertex>& out_verts,
                        std::vector<uint32_t>& out_idx);

bool SafeExtractArrays(GLint first, GLsizei count, const TrackedGLState& st,
                       std::vector<SharedVertex>& out_verts,
                       std::vector<uint32_t>& out_idx);
