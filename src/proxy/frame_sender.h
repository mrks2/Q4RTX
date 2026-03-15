#pragma once
#include "gl_state.h"
#include <cstdint>

namespace frame_sender {

bool Init();
void Shutdown();

void SetViewMatrix(const float* view_mat);

void OnDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices,
                    const TrackedGLState& state, const float* modelview);

void OnDrawArrays(GLenum mode, GLint first, GLsizei count,
                  const TrackedGLState& state, const float* modelview);

void AddLight(const float* light_origin_local, const float* light_color, const float* modelview);

void FlushFrame(uint32_t frame_number, const float* view_mat, const float* proj_mat);

} // namespace frame_sender
