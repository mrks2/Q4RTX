#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>

struct GLProxyGlobals {
    HMODULE realOpenGL32 = nullptr;

    #define DECL_REAL(ret, name, ...) ret (APIENTRY *real_##name)(__VA_ARGS__) = nullptr
    #define DECL_REAL_W(ret, name, ...) ret (WINAPI *real_##name)(__VA_ARGS__) = nullptr

    // WGL
    DECL_REAL_W(HGLRC,  wglCreateContext,       HDC);
    DECL_REAL_W(BOOL,   wglDeleteContext,        HGLRC);
    DECL_REAL_W(BOOL,   wglMakeCurrent,          HDC, HGLRC);
    DECL_REAL_W(PROC,   wglGetProcAddress,       LPCSTR);
    DECL_REAL_W(HGLRC,  wglGetCurrentContext);
    DECL_REAL_W(HDC,    wglGetCurrentDC);
    DECL_REAL_W(BOOL,   wglShareLists,           HGLRC, HGLRC);
    DECL_REAL_W(BOOL,   wglSwapBuffers,          HDC);
    DECL_REAL_W(BOOL,   wglSwapLayerBuffers,     HDC, UINT);

    // Draw
    DECL_REAL(void, glDrawElements,    unsigned, int, unsigned, const void*);
    DECL_REAL(void, glDrawArrays,      unsigned, int, int);

    // Immediate mode
    DECL_REAL(void, glBegin,           unsigned);
    DECL_REAL(void, glEnd);
    DECL_REAL(void, glVertex3f,        float, float, float);
    DECL_REAL(void, glVertex3fv,       const float*);
    DECL_REAL(void, glVertex2f,        float, float);
    DECL_REAL(void, glNormal3f,        float, float, float);
    DECL_REAL(void, glNormal3fv,       const float*);
    DECL_REAL(void, glTexCoord2f,      float, float);
    DECL_REAL(void, glTexCoord2fv,     const float*);
    DECL_REAL(void, glColor3f,         float, float, float);
    DECL_REAL(void, glColor4f,         float, float, float, float);
    DECL_REAL(void, glColor4fv,        const float*);
    DECL_REAL(void, glColor3ub,        unsigned char, unsigned char, unsigned char);
    DECL_REAL(void, glColor4ub,        unsigned char, unsigned char, unsigned char, unsigned char);
    DECL_REAL(void, glColor4ubv,       const unsigned char*);

    // Textures
    DECL_REAL(void, glBindTexture,     unsigned, unsigned);
    DECL_REAL(void, glTexImage2D,      unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
    DECL_REAL(void, glTexSubImage2D,   unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
    DECL_REAL(void, glGenTextures,     int, unsigned*);
    DECL_REAL(void, glDeleteTextures,  int, const unsigned*);
    DECL_REAL(void, glTexParameteri,   unsigned, unsigned, int);
    DECL_REAL(void, glTexParameterf,   unsigned, unsigned, float);

    // State
    DECL_REAL(void, glEnable,          unsigned);
    DECL_REAL(void, glDisable,         unsigned);
    DECL_REAL(void, glBlendFunc,       unsigned, unsigned);
    DECL_REAL(void, glDepthFunc,       unsigned);
    DECL_REAL(void, glDepthMask,       unsigned char);
    DECL_REAL(void, glAlphaFunc,       unsigned, float);
    DECL_REAL(void, glColorMask,       unsigned char, unsigned char, unsigned char, unsigned char);
    DECL_REAL(void, glStencilFunc,     unsigned, int, unsigned);
    DECL_REAL(void, glStencilOp,       unsigned, unsigned, unsigned);
    DECL_REAL(void, glStencilMask,     unsigned);
    DECL_REAL(void, glCullFace,        unsigned);
    DECL_REAL(void, glPolygonMode,     unsigned, unsigned);
    DECL_REAL(void, glPolygonOffset,   float, float);
    DECL_REAL(void, glScissor,         int, int, int, int);
    DECL_REAL(void, glDepthRange,      double, double);

    // Matrix
    DECL_REAL(void, glMatrixMode,      unsigned);
    DECL_REAL(void, glLoadIdentity);
    DECL_REAL(void, glLoadMatrixf,     const float*);
    DECL_REAL(void, glMultMatrixf,     const float*);
    DECL_REAL(void, glPushMatrix);
    DECL_REAL(void, glPopMatrix);
    DECL_REAL(void, glOrtho,           double, double, double, double, double, double);
    DECL_REAL(void, glFrustum,         double, double, double, double, double, double);

    // Viewport / clear / misc
    DECL_REAL(void,          glViewport,     int, int, int, int);
    DECL_REAL(void,          glClear,        unsigned);
    DECL_REAL(void,          glClearColor,   float, float, float, float);
    DECL_REAL(void,          glClearDepth,   double);
    DECL_REAL(void,          glReadBuffer,   unsigned);
    DECL_REAL(void,          glReadPixels,   int, int, int, int, unsigned, unsigned, void*);
    DECL_REAL(void,          glFinish);
    DECL_REAL(void,          glFlush);
    DECL_REAL(unsigned,      glGetError);
    DECL_REAL(void,          glGetIntegerv,  unsigned, int*);
    DECL_REAL(void,          glGetFloatv,    unsigned, float*);
    DECL_REAL(const unsigned char*, glGetString, unsigned);
    DECL_REAL(unsigned char, glIsEnabled,    unsigned);
    DECL_REAL(void,          glPixelStorei,  unsigned, int);
    DECL_REAL(void,          glHint,         unsigned, unsigned);

    // Client state / vertex arrays
    DECL_REAL(void, glEnableClientState,  unsigned);
    DECL_REAL(void, glDisableClientState, unsigned);
    DECL_REAL(void, glVertexPointer,      int, unsigned, int, const void*);
    DECL_REAL(void, glNormalPointer,      unsigned, int, const void*);
    DECL_REAL(void, glTexCoordPointer,    int, unsigned, int, const void*);
    DECL_REAL(void, glColorPointer,       int, unsigned, int, const void*);

    // Attribs
    DECL_REAL(void, glPushAttrib,    unsigned);
    DECL_REAL(void, glPopAttrib);
    DECL_REAL(void, glLineWidth,     float);
    DECL_REAL(void, glPointSize,     float);

    #undef DECL_REAL
    #undef DECL_REAL_W
};

extern GLProxyGlobals g_gl;

extern "C" void glPassthroughInit();
