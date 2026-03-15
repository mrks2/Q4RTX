// ARB extension hooks -- intercepted via wglGetProcAddress.
//
// We hook these to mirror VBO/EBO contents on the CPU for geometry extraction,
// capture light parameters from ARB program params, and track vertex attrib
// and texture unit state.

#include "gl_proxy.h"
#include "gl_hooks_internal.h"
#include <gl/GL.h>
#include <cstring>
#include <cstdio>

// Light state (read by draw hooks in gl_hooks.cpp)
float    g_curLightOriginLocal[4] = {};
float    g_curLightColor[4]       = {};
bool     g_lightOriginSet         = false;
bool     g_lightColorSet          = false;
uint32_t g_envParamCalls          = 0;
uint32_t g_localParamCalls        = 0;

#define GL_VERTEX_PROGRAM_ARB   0x8620
#define GL_FRAGMENT_PROGRAM_ARB 0x8804

// VBO/EBO

typedef void (APIENTRY* PFN_glBindBufferARB)(GLenum, GLuint);
typedef void (APIENTRY* PFN_glBufferDataARB)(GLenum, int, const void*, GLenum);
typedef void (APIENTRY* PFN_glBufferSubDataARB)(GLenum, int, int, const void*);
typedef void (APIENTRY* PFN_glDeleteBuffersARB)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFN_glGenBuffersARB)(GLsizei, GLuint*);

static PFN_glBindBufferARB    real_glBindBufferARB    = nullptr;
static PFN_glBufferDataARB    real_glBufferDataARB    = nullptr;
static PFN_glBufferSubDataARB real_glBufferSubDataARB = nullptr;
static PFN_glDeleteBuffersARB real_glDeleteBuffersARB = nullptr;
static PFN_glGenBuffersARB    real_glGenBuffersARB    = nullptr;

static void APIENTRY hook_glBindBufferARB(GLenum target, GLuint buffer) {
    g_vboTracker.BindBuffer(target, buffer);
    real_glBindBufferARB(target, buffer);
}

static void APIENTRY hook_glBufferDataARB(GLenum target, int size, const void* data, GLenum usage) {
    g_vboTracker.BufferUpload(target, size, data, usage);
    real_glBufferDataARB(target, size, data, usage);
}

static void APIENTRY hook_glBufferSubDataARB(GLenum target, int offset, int size, const void* data) {
    g_vboTracker.BufferSubData(target, offset, size, data);
    real_glBufferSubDataARB(target, offset, size, data);
}

static void APIENTRY hook_glDeleteBuffersARB(GLsizei n, const GLuint* buffers) {
    g_vboTracker.DeleteBuffers(n, buffers);
    real_glDeleteBuffersARB(n, buffers);
}

static void APIENTRY hook_glGenBuffersARB(GLsizei n, GLuint* buffers) {
    real_glGenBuffersARB(n, buffers);
}

// Program parameter hooks for light capture.
// id Tech 4 passes light origin as VP param 0, light color as FP param 0.

typedef void (APIENTRY* PFN_glProgramLocalParameter4fvARB)(GLenum, GLuint, const GLfloat*);
typedef void (APIENTRY* PFN_glProgramLocalParameter4fARB)(GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY* PFN_glProgramEnvParameter4fvARB)(GLenum, GLuint, const GLfloat*);
typedef void (APIENTRY* PFN_glProgramEnvParameter4fARB)(GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);

static PFN_glProgramLocalParameter4fvARB real_glProgramLocalParameter4fvARB = nullptr;
static PFN_glProgramLocalParameter4fARB  real_glProgramLocalParameter4fARB  = nullptr;
static PFN_glProgramEnvParameter4fvARB   real_glProgramEnvParameter4fvARB   = nullptr;
static PFN_glProgramEnvParameter4fARB    real_glProgramEnvParameter4fARB    = nullptr;

static void APIENTRY hook_glProgramLocalParameter4fvARB(GLenum target, GLuint index, const GLfloat* params) {
    g_localParamCalls++;
    if (target == GL_VERTEX_PROGRAM_ARB && index == 0) {
        memcpy(g_curLightOriginLocal, params, 16);
        g_lightOriginSet = true;
    }
    if (target == GL_FRAGMENT_PROGRAM_ARB && index == 0) {
        memcpy(g_curLightColor, params, 16);
        g_lightColorSet = true;
    }
    real_glProgramLocalParameter4fvARB(target, index, params);
}

static void APIENTRY hook_glProgramLocalParameter4fARB(GLenum target, GLuint index,
                                                        GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    g_localParamCalls++;
    if (target == GL_VERTEX_PROGRAM_ARB && index == 0) {
        g_curLightOriginLocal[0] = x; g_curLightOriginLocal[1] = y;
        g_curLightOriginLocal[2] = z; g_curLightOriginLocal[3] = w;
        g_lightOriginSet = true;
    }
    if (target == GL_FRAGMENT_PROGRAM_ARB && index == 0) {
        g_curLightColor[0] = x; g_curLightColor[1] = y;
        g_curLightColor[2] = z; g_curLightColor[3] = w;
        g_lightColorSet = true;
    }
    real_glProgramLocalParameter4fARB(target, index, x, y, z, w);
}

static void APIENTRY hook_glProgramEnvParameter4fvARB(GLenum target, GLuint index, const GLfloat* params) {
    g_envParamCalls++;
    if (target == GL_VERTEX_PROGRAM_ARB && index == 0) {
        memcpy(g_curLightOriginLocal, params, 16);
        g_lightOriginSet = true;
    }
    if (target == GL_FRAGMENT_PROGRAM_ARB && index == 0) {
        memcpy(g_curLightColor, params, 16);
        g_lightColorSet = true;
    }
    real_glProgramEnvParameter4fvARB(target, index, params);
}

static void APIENTRY hook_glProgramEnvParameter4fARB(GLenum target, GLuint index,
                                                      GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    g_envParamCalls++;
    if (target == GL_VERTEX_PROGRAM_ARB && index == 0) {
        g_curLightOriginLocal[0] = x; g_curLightOriginLocal[1] = y;
        g_curLightOriginLocal[2] = z; g_curLightOriginLocal[3] = w;
        g_lightOriginSet = true;
    }
    if (target == GL_FRAGMENT_PROGRAM_ARB && index == 0) {
        g_curLightColor[0] = x; g_curLightColor[1] = y;
        g_curLightColor[2] = z; g_curLightColor[3] = w;
        g_lightColorSet = true;
    }
    real_glProgramEnvParameter4fARB(target, index, x, y, z, w);
}

// Vertex attribs -- id Tech 4 uses attrib 8 for texcoords (ATTR_INDEX_ST)

typedef void (APIENTRY* PFN_glVertexAttribPointerARB)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void (APIENTRY* PFN_glEnableVertexAttribArrayARB)(GLuint);
typedef void (APIENTRY* PFN_glDisableVertexAttribArrayARB)(GLuint);

static PFN_glVertexAttribPointerARB      real_glVertexAttribPointerARB      = nullptr;
static PFN_glEnableVertexAttribArrayARB   real_glEnableVertexAttribArrayARB   = nullptr;
static PFN_glDisableVertexAttribArrayARB  real_glDisableVertexAttribArrayARB  = nullptr;

static void APIENTRY hook_glVertexAttribPointerARB(GLuint index, GLint size, GLenum type,
                                                     GLboolean normalized, GLsizei stride, const void* ptr) {
    if (index == 8) {
        g_st.tc_ptr = ptr; g_st.tc_size = size;
        g_st.tc_type = type; g_st.tc_stride = stride;
    }
    real_glVertexAttribPointerARB(index, size, type, normalized, stride, ptr);
}

static void APIENTRY hook_glEnableVertexAttribArrayARB(GLuint index) {
    if (index == 8) g_st.tc_array_on = true;
    real_glEnableVertexAttribArrayARB(index);
}

static void APIENTRY hook_glDisableVertexAttribArrayARB(GLuint index) {
    if (index == 8) g_st.tc_array_on = false;
    real_glDisableVertexAttribArrayARB(index);
}

// Active texture -- id Tech 4 binds diffuse on unit 4 during interaction passes

typedef void (APIENTRY* PFN_glActiveTextureARB)(GLenum);
static PFN_glActiveTextureARB real_glActiveTextureARB = nullptr;

static void APIENTRY hook_glActiveTextureARB(GLenum texture) {
    uint32_t unit = texture - 0x84C0; // GL_TEXTURE0
    if (unit < 8) g_st.active_texture_unit = unit;
    real_glActiveTextureARB(texture);
}

// wglGetProcAddress -- return our hooks for known extensions, real pointers otherwise

extern "C" {

PROC WINAPI hooked_wglGetProcAddress(LPCSTR name) {
    PROC real = g_gl.real_wglGetProcAddress(name);
    if (!real) return nullptr;

    #define INTERCEPT(fn) \
        if (strcmp(name, #fn) == 0 || strcmp(name, #fn "ARB") == 0) { \
            real_##fn##ARB = (PFN_##fn##ARB)real; \
            return (PROC)hook_##fn##ARB; \
        }

    INTERCEPT(glBindBuffer)
    INTERCEPT(glBufferData)
    INTERCEPT(glBufferSubData)
    INTERCEPT(glDeleteBuffers)
    INTERCEPT(glGenBuffers)
    INTERCEPT(glProgramLocalParameter4fv)
    INTERCEPT(glProgramLocalParameter4f)
    INTERCEPT(glProgramEnvParameter4fv)
    INTERCEPT(glProgramEnvParameter4f)
    INTERCEPT(glVertexAttribPointer)
    INTERCEPT(glEnableVertexAttribArray)
    INTERCEPT(glDisableVertexAttribArray)
    INTERCEPT(glActiveTexture)
    #undef INTERCEPT

    return real;
}

} // extern "C"
