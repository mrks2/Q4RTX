// GL state tracking hooks -- enable/disable, blend, depth, matrices, textures,
// client state, vertex arrays, immediate mode, viewport, etc.

#include "gl_proxy.h"
#include "gl_hooks_internal.h"
#include "frame_sender.h"
#include <gl/GL.h>
#include <cstring>

TrackedGLState g_st;
VBOTracker     g_vboTracker;

float g_viewMatrix[16]  = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
float g_projMatrix[16]  = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
bool  g_viewCaptured    = false;

extern "C" {

// WGL passthrough
HGLRC WINAPI hooked_wglCreateContext(HDC hdc)           { return g_gl.real_wglCreateContext(hdc); }
BOOL  WINAPI hooked_wglDeleteContext(HGLRC hrc)         { return g_gl.real_wglDeleteContext(hrc); }
BOOL  WINAPI hooked_wglMakeCurrent(HDC hdc, HGLRC hrc)  { return g_gl.real_wglMakeCurrent(hdc, hrc); }
HGLRC WINAPI hooked_wglGetCurrentContext()               { return g_gl.real_wglGetCurrentContext(); }
HDC   WINAPI hooked_wglGetCurrentDC()                    { return g_gl.real_wglGetCurrentDC(); }
BOOL  WINAPI hooked_wglShareLists(HGLRC a, HGLRC b)     { return g_gl.real_wglShareLists(a, b); }

BOOL WINAPI hooked_wglSwapLayerBuffers(HDC hdc, UINT planes) {
    return g_gl.real_wglSwapLayerBuffers(hdc, planes);
}

void APIENTRY hooked_glEnable(GLenum cap) {
    switch (cap) {
        case GL_DEPTH_TEST: g_st.depth_test = true; break;
        case GL_BLEND:      g_st.blend = true; break;
        case GL_TEXTURE_2D: g_st.texture_2d = true; break;
        case GL_ALPHA_TEST: g_st.alpha_test = true; break;
    }
    g_gl.real_glEnable(cap);
}

void APIENTRY hooked_glDisable(GLenum cap) {
    switch (cap) {
        case GL_DEPTH_TEST: g_st.depth_test = false; break;
        case GL_BLEND:      g_st.blend = false; break;
        case GL_TEXTURE_2D: g_st.texture_2d = false; break;
        case GL_ALPHA_TEST: g_st.alpha_test = false; break;
    }
    g_gl.real_glDisable(cap);
}

void APIENTRY hooked_glBlendFunc(GLenum src, GLenum dst) {
    g_st.blend_src = src; g_st.blend_dst = dst;
    g_gl.real_glBlendFunc(src, dst);
}
void APIENTRY hooked_glDepthFunc(GLenum func)      { g_gl.real_glDepthFunc(func); }
void APIENTRY hooked_glDepthMask(GLboolean flag)    { g_st.depth_write = flag; g_gl.real_glDepthMask(flag); }
void APIENTRY hooked_glDepthRange(GLdouble near_val, GLdouble far_val) {
    g_st.depth_range_near = near_val;
    g_st.depth_range_far  = far_val;
    g_gl.real_glDepthRange(near_val, far_val);
}
void APIENTRY hooked_glAlphaFunc(GLenum func, GLclampf ref)                              { g_gl.real_glAlphaFunc(func, ref); }
void APIENTRY hooked_glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)     { g_gl.real_glColorMask(r,g,b,a); }
void APIENTRY hooked_glStencilFunc(GLenum func, GLint ref, GLuint mask)                  { g_gl.real_glStencilFunc(func, ref, mask); }
void APIENTRY hooked_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)                { g_gl.real_glStencilOp(fail, zfail, zpass); }
void APIENTRY hooked_glStencilMask(GLuint mask)                                          { g_gl.real_glStencilMask(mask); }
void APIENTRY hooked_glCullFace(GLenum mode)                                             { g_gl.real_glCullFace(mode); }
void APIENTRY hooked_glPolygonMode(GLenum face, GLenum mode)                             { g_gl.real_glPolygonMode(face, mode); }
void APIENTRY hooked_glPolygonOffset(GLfloat factor, GLfloat units)                      { g_gl.real_glPolygonOffset(factor, units); }
void APIENTRY hooked_glScissor(GLint x, GLint y, GLsizei w, GLsizei h)                  { g_gl.real_glScissor(x, y, w, h); }

// Matrix tracking

void APIENTRY hooked_glMatrixMode(GLenum mode) {
    g_st.matrix_mode = mode;
    g_gl.real_glMatrixMode(mode);
}

void APIENTRY hooked_glLoadIdentity() {
    float id[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    if (g_st.matrix_mode == GL_MODELVIEW)  memcpy(g_st.modelview, id, 64);
    if (g_st.matrix_mode == GL_PROJECTION) memcpy(g_st.projection, id, 64);
    g_gl.real_glLoadIdentity();
}

void APIENTRY hooked_glLoadMatrixf(const GLfloat* m) {
    if (g_st.matrix_mode == GL_MODELVIEW) {
        memcpy(g_st.modelview, m, 64);
        // first MODELVIEW load while not in ortho = the view matrix
        if (!g_viewCaptured && !g_st.in_ortho) {
            memcpy(g_viewMatrix, m, 64);
            frame_sender::SetViewMatrix(m);
            g_viewCaptured = true;
        }
    }
    if (g_st.matrix_mode == GL_PROJECTION) {
        memcpy(g_st.projection, m, 64);
        // only capture perspective projections, not ortho via glLoadMatrixf
        // perspective: m[15]==0, m[11]==-1 ; ortho: m[15]==1, m[11]==0
        if (m[15] == 0.0f && m[11] == -1.0f) {
            memcpy(g_projMatrix, m, 64);
        }
    }
    g_gl.real_glLoadMatrixf(m);
}

void APIENTRY hooked_glMultMatrixf(const GLfloat* m) {
    if (g_st.matrix_mode == GL_MODELVIEW) {
        float tmp[16];
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++)
                tmp[col*4+row] = g_st.modelview[0*4+row]*m[col*4+0]
                               + g_st.modelview[1*4+row]*m[col*4+1]
                               + g_st.modelview[2*4+row]*m[col*4+2]
                               + g_st.modelview[3*4+row]*m[col*4+3];
        memcpy(g_st.modelview, tmp, 64);
    }
    g_gl.real_glMultMatrixf(m);
}

void APIENTRY hooked_glPushMatrix() {
    if (g_st.matrix_mode == GL_MODELVIEW && g_st.mv_depth < TrackedGLState::MAX_MV_STACK - 1) {
        memcpy(g_st.mv_stack[g_st.mv_depth], g_st.modelview, 64);
        g_st.mv_depth++;
    }
    g_gl.real_glPushMatrix();
}

void APIENTRY hooked_glPopMatrix() {
    if (g_st.matrix_mode == GL_MODELVIEW && g_st.mv_depth > 0) {
        g_st.mv_depth--;
        memcpy(g_st.modelview, g_st.mv_stack[g_st.mv_depth], 64);
    }
    g_gl.real_glPopMatrix();
}

void APIENTRY hooked_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    g_st.in_ortho = true;
    g_gl.real_glOrtho(l, r, b, t, n, f);
}

void APIENTRY hooked_glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    g_gl.real_glFrustum(l, r, b, t, n, f);
}

// Textures

void APIENTRY hooked_glBindTexture(GLenum target, GLuint tex) {
    if (target == GL_TEXTURE_2D) {
        if (g_st.active_texture_unit < 8)
            g_st.bound_texture_2d[g_st.active_texture_unit] = tex;
        // prefer unit 4 (id Tech 4 interaction pass diffuse), fallback to unit 0
        g_st.bound_texture = g_st.bound_texture_2d[4] ? g_st.bound_texture_2d[4] : g_st.bound_texture_2d[0];
    }
    g_gl.real_glBindTexture(target, tex);
}
void APIENTRY hooked_glTexImage2D(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void* d) { g_gl.real_glTexImage2D(t,l,i,w,h,b,f,ty,d); }
void APIENTRY hooked_glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void* d) { g_gl.real_glTexSubImage2D(t,l,x,y,w,h,f,ty,d); }
void APIENTRY hooked_glGenTextures(GLsizei n, GLuint* t)          { g_gl.real_glGenTextures(n, t); }
void APIENTRY hooked_glDeleteTextures(GLsizei n, const GLuint* t) { g_gl.real_glDeleteTextures(n, t); }
void APIENTRY hooked_glTexParameteri(GLenum t, GLenum p, GLint v)  { g_gl.real_glTexParameteri(t, p, v); }
void APIENTRY hooked_glTexParameterf(GLenum t, GLenum p, GLfloat v) { g_gl.real_glTexParameterf(t, p, v); }

void APIENTRY hooked_glViewport(GLint x, GLint y, GLsizei w, GLsizei h)                            { g_gl.real_glViewport(x, y, w, h); }
void APIENTRY hooked_glClear(GLbitfield mask)                                                       { g_gl.real_glClear(mask); }
void APIENTRY hooked_glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)                   { g_gl.real_glClearColor(r,g,b,a); }
void APIENTRY hooked_glClearDepth(GLclampd d)                                                       { g_gl.real_glClearDepth(d); }
void APIENTRY hooked_glReadBuffer(GLenum mode)                                                      { g_gl.real_glReadBuffer(mode); }
void APIENTRY hooked_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void* d) { g_gl.real_glReadPixels(x,y,w,h,f,t,d); }
void APIENTRY hooked_glFinish()                                                                     { g_gl.real_glFinish(); }
void APIENTRY hooked_glFlush()                                                                      { g_gl.real_glFlush(); }
GLenum APIENTRY hooked_glGetError()                                                                 { return g_gl.real_glGetError(); }
void APIENTRY hooked_glGetIntegerv(GLenum pname, GLint* params)                                     { g_gl.real_glGetIntegerv(pname, params); }
void APIENTRY hooked_glGetFloatv(GLenum pname, GLfloat* params)                                     { g_gl.real_glGetFloatv(pname, params); }
const GLubyte* APIENTRY hooked_glGetString(GLenum name)                                             { return g_gl.real_glGetString(name); }
GLboolean APIENTRY hooked_glIsEnabled(GLenum cap)                                                   { return g_gl.real_glIsEnabled(cap); }
void APIENTRY hooked_glPixelStorei(GLenum pname, GLint param)                                       { g_gl.real_glPixelStorei(pname, param); }
void APIENTRY hooked_glHint(GLenum target, GLenum mode)                                             { g_gl.real_glHint(target, mode); }

// Client state / vertex arrays

void APIENTRY hooked_glEnableClientState(GLenum array) {
    if (array == GL_VERTEX_ARRAY)         g_st.vert_array_on = true;
    if (array == GL_NORMAL_ARRAY)         g_st.norm_array_on = true;
    if (array == GL_TEXTURE_COORD_ARRAY)  g_st.tc_array_on = true;
    g_gl.real_glEnableClientState(array);
}

void APIENTRY hooked_glDisableClientState(GLenum array) {
    if (array == GL_VERTEX_ARRAY)         g_st.vert_array_on = false;
    if (array == GL_NORMAL_ARRAY)         g_st.norm_array_on = false;
    if (array == GL_TEXTURE_COORD_ARRAY)  g_st.tc_array_on = false;
    g_gl.real_glDisableClientState(array);
}

void APIENTRY hooked_glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) {
    g_st.vert_ptr = ptr; g_st.vert_size = size;
    g_st.vert_type = type; g_st.vert_stride = stride;
    g_gl.real_glVertexPointer(size, type, stride, ptr);
}

void APIENTRY hooked_glNormalPointer(GLenum type, GLsizei stride, const void* ptr) {
    g_st.norm_ptr = ptr; g_st.norm_type = type; g_st.norm_stride = stride;
    g_gl.real_glNormalPointer(type, stride, ptr);
}

void APIENTRY hooked_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) {
    g_st.tc_ptr = ptr; g_st.tc_size = size;
    g_st.tc_type = type; g_st.tc_stride = stride;
    g_gl.real_glTexCoordPointer(size, type, stride, ptr);
}

void APIENTRY hooked_glColorPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) {
    g_gl.real_glColorPointer(size, type, stride, ptr);
}

// Immediate mode passthrough
void APIENTRY hooked_glBegin(GLenum mode)                                                      { g_gl.real_glBegin(mode); }
void APIENTRY hooked_glEnd()                                                                    { g_gl.real_glEnd(); }
void APIENTRY hooked_glVertex3f(float x, float y, float z)                                      { g_gl.real_glVertex3f(x, y, z); }
void APIENTRY hooked_glVertex3fv(const float* v)                                                { g_gl.real_glVertex3fv(v); }
void APIENTRY hooked_glVertex2f(float x, float y)                                               { g_gl.real_glVertex2f(x, y); }
void APIENTRY hooked_glNormal3f(float x, float y, float z)                                      { g_gl.real_glNormal3f(x, y, z); }
void APIENTRY hooked_glNormal3fv(const float* v)                                                { g_gl.real_glNormal3fv(v); }
void APIENTRY hooked_glTexCoord2f(float s, float t)                                              { g_gl.real_glTexCoord2f(s, t); }
void APIENTRY hooked_glTexCoord2fv(const float* v)                                              { g_gl.real_glTexCoord2fv(v); }
void APIENTRY hooked_glColor3f(float r, float g, float b)                                       { g_gl.real_glColor3f(r, g, b); }
void APIENTRY hooked_glColor4f(float r, float g, float b, float a)                              { g_gl.real_glColor4f(r, g, b, a); }
void APIENTRY hooked_glColor4fv(const float* v)                                                 { g_gl.real_glColor4fv(v); }
void APIENTRY hooked_glColor3ub(GLubyte r, GLubyte g, GLubyte b)                                 { g_gl.real_glColor3ub(r, g, b); }
void APIENTRY hooked_glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)                      { g_gl.real_glColor4ub(r, g, b, a); }
void APIENTRY hooked_glColor4ubv(const GLubyte* v)                                              { g_gl.real_glColor4ubv(v); }

void APIENTRY hooked_glPushAttrib(GLbitfield mask) { g_gl.real_glPushAttrib(mask); }
void APIENTRY hooked_glPopAttrib()                 { g_gl.real_glPopAttrib(); }
void APIENTRY hooked_glLineWidth(GLfloat width)    { g_gl.real_glLineWidth(width); }
void APIENTRY hooked_glPointSize(GLfloat size)     { g_gl.real_glPointSize(size); }

} // extern "C"
