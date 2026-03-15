#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <gl/GL.h>

struct TrackedGLState {
    bool depth_test   = false;
    bool blend        = false;
    bool texture_2d   = false;
    bool alpha_test   = false;
    bool in_ortho     = false;

    GLenum blend_src  = GL_ONE;
    GLenum blend_dst  = GL_ZERO;
    GLenum matrix_mode = GL_MODELVIEW;
    unsigned char depth_write = 1;

    const void* vert_ptr     = nullptr;
    int         vert_size    = 3;
    GLenum      vert_type    = GL_FLOAT;
    int         vert_stride  = 0;

    const void* norm_ptr     = nullptr;
    GLenum      norm_type    = GL_FLOAT;
    int         norm_stride  = 0;

    // from glTexCoordPointer or glVertexAttribPointerARB index 8
    const void* tc_ptr       = nullptr;
    int         tc_size      = 2;
    GLenum      tc_type      = GL_FLOAT;
    int         tc_stride    = 0;

    bool vert_array_on  = false;
    bool norm_array_on  = false;
    bool tc_array_on    = false;

    GLenum active_texture_unit = 0; // 0-based
    GLuint bound_texture_2d[8] = {};

    GLuint bound_texture = 0; // best guess at current diffuse

    double depth_range_near = 0.0;
    double depth_range_far  = 1.0;

    static constexpr int MAX_MV_STACK = 32;
    float modelview[16]  = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float projection[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float mv_stack[MAX_MV_STACK][16] = {};
    int   mv_depth = 0;

    uint32_t frame = 0;
};
