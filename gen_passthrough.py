"""Generate gl_passthrough.cpp with forwarding stubs for all GL functions
that Quake 4 imports but we don't need to hook."""

import sys
import os

# All passthrough functions (core GL/WGL that we don't hook)
passthrough = [
    'glAccum', 'glAreTexturesResident', 'glArrayElement', 'glBitmap',
    'glCallList', 'glCallLists', 'glClearAccum', 'glClearIndex', 'glClearStencil',
    'glClipPlane', 'glColor3b', 'glColor3bv', 'glColor3d', 'glColor3dv',
    'glColor3fv', 'glColor3i', 'glColor3iv', 'glColor3s', 'glColor3sv',
    'glColor3ubv', 'glColor3ui', 'glColor3uiv', 'glColor3us', 'glColor3usv',
    'glColor4b', 'glColor4bv', 'glColor4d', 'glColor4dv',
    'glColor4i', 'glColor4iv', 'glColor4s', 'glColor4sv',
    'glColor4ui', 'glColor4uiv', 'glColor4us', 'glColor4usv',
    'glColorMaterial', 'glCopyPixels', 'glCopyTexImage1D', 'glCopyTexImage2D',
    'glCopyTexSubImage1D', 'glCopyTexSubImage2D', 'glDeleteLists', 'glDepthRange',
    'glDrawBuffer', 'glDrawPixels', 'glEdgeFlag', 'glEdgeFlagPointer', 'glEdgeFlagv',
    'glEndList', 'glEvalCoord1d', 'glEvalCoord1dv', 'glEvalCoord1f', 'glEvalCoord1fv',
    'glEvalCoord2d', 'glEvalCoord2dv', 'glEvalCoord2f', 'glEvalCoord2fv',
    'glEvalMesh1', 'glEvalMesh2', 'glEvalPoint1', 'glEvalPoint2',
    'glFeedbackBuffer', 'glFogf', 'glFogfv', 'glFogi', 'glFogiv',
    'glFrontFace', 'glGenLists',
    'glGetBooleanv', 'glGetClipPlane', 'glGetDoublev',
    'glGetLightfv', 'glGetLightiv', 'glGetMapdv', 'glGetMapfv', 'glGetMapiv',
    'glGetMaterialfv', 'glGetMaterialiv', 'glGetPixelMapfv', 'glGetPixelMapuiv',
    'glGetPixelMapusv', 'glGetPointerv', 'glGetPolygonStipple',
    'glGetTexEnvfv', 'glGetTexEnviv', 'glGetTexGendv', 'glGetTexGenfv', 'glGetTexGeniv',
    'glGetTexImage', 'glGetTexLevelParameterfv', 'glGetTexLevelParameteriv',
    'glGetTexParameterfv', 'glGetTexParameteriv',
    'glIndexMask', 'glIndexPointer', 'glIndexd', 'glIndexdv', 'glIndexf', 'glIndexfv',
    'glIndexi', 'glIndexiv', 'glIndexs', 'glIndexsv', 'glIndexub', 'glIndexubv',
    'glInitNames', 'glInterleavedArrays', 'glIsList', 'glIsTexture',
    'glLightModelf', 'glLightModelfv', 'glLightModeli', 'glLightModeliv',
    'glLightf', 'glLightfv', 'glLighti', 'glLightiv',
    'glLineStipple', 'glListBase', 'glLoadMatrixd', 'glLoadName', 'glLogicOp',
    'glMap1d', 'glMap1f', 'glMap2d', 'glMap2f',
    'glMapGrid1d', 'glMapGrid1f', 'glMapGrid2d', 'glMapGrid2f',
    'glMaterialf', 'glMaterialfv', 'glMateriali', 'glMaterialiv',
    'glMultMatrixd', 'glNewList',
    'glNormal3b', 'glNormal3bv', 'glNormal3d', 'glNormal3dv',
    'glNormal3i', 'glNormal3iv', 'glNormal3s', 'glNormal3sv',
    'glPassThrough', 'glPixelMapfv', 'glPixelMapuiv', 'glPixelMapusv',
    'glPixelStoref', 'glPixelTransferf', 'glPixelTransferi', 'glPixelZoom',
    'glPolygonStipple', 'glPopClientAttrib', 'glPopName',
    'glPrioritizeTextures', 'glPushClientAttrib', 'glPushName',
    'glRasterPos2d', 'glRasterPos2dv', 'glRasterPos2f', 'glRasterPos2fv',
    'glRasterPos2i', 'glRasterPos2iv', 'glRasterPos2s', 'glRasterPos2sv',
    'glRasterPos3d', 'glRasterPos3dv', 'glRasterPos3f', 'glRasterPos3fv',
    'glRasterPos3i', 'glRasterPos3iv', 'glRasterPos3s', 'glRasterPos3sv',
    'glRasterPos4d', 'glRasterPos4dv', 'glRasterPos4f', 'glRasterPos4fv',
    'glRasterPos4i', 'glRasterPos4iv', 'glRasterPos4s', 'glRasterPos4sv',
    'glRectd', 'glRectdv', 'glRectf', 'glRectfv', 'glRecti', 'glRectiv',
    'glRects', 'glRectsv', 'glRenderMode',
    'glRotated', 'glRotatef', 'glScaled', 'glScalef',
    'glSelectBuffer', 'glShadeModel',
    'glTexCoord1d', 'glTexCoord1dv', 'glTexCoord1f', 'glTexCoord1fv',
    'glTexCoord1i', 'glTexCoord1iv', 'glTexCoord1s', 'glTexCoord1sv',
    'glTexCoord2d', 'glTexCoord2dv', 'glTexCoord2i', 'glTexCoord2iv',
    'glTexCoord2s', 'glTexCoord2sv',
    'glTexCoord3d', 'glTexCoord3dv', 'glTexCoord3f', 'glTexCoord3fv',
    'glTexCoord3i', 'glTexCoord3iv', 'glTexCoord3s', 'glTexCoord3sv',
    'glTexCoord4d', 'glTexCoord4dv', 'glTexCoord4f', 'glTexCoord4fv',
    'glTexCoord4i', 'glTexCoord4iv', 'glTexCoord4s', 'glTexCoord4sv',
    'glTexEnvf', 'glTexEnvfv', 'glTexEnvi', 'glTexEnviv',
    'glTexGend', 'glTexGendv', 'glTexGenf', 'glTexGenfv', 'glTexGeni', 'glTexGeniv',
    'glTexImage1D', 'glTexImage3D', 'glTexParameterfv', 'glTexParameteriv',
    'glTexSubImage1D', 'glTranslated', 'glTranslatef',
    'glVertex2d', 'glVertex2dv', 'glVertex2fv', 'glVertex2i', 'glVertex2iv',
    'glVertex2s', 'glVertex2sv', 'glVertex3d', 'glVertex3dv',
    'glVertex3i', 'glVertex3iv', 'glVertex3s', 'glVertex3sv',
    'glVertex4d', 'glVertex4dv', 'glVertex4f', 'glVertex4fv',
    'glVertex4i', 'glVertex4iv', 'glVertex4s', 'glVertex4sv',
    'wglCopyContext', 'wglCreateLayerContext', 'wglDescribeLayerPlane',
    'wglGetLayerPaletteEntries', 'wglRealizeLayerPalette',
    'wglSetLayerPaletteEntries', 'wglUseFontBitmapsA', 'wglUseFontOutlinesA',
    # Critical ICD functions — must be forwarded for wglCreateContext to work
    'wglChoosePixelFormat', 'wglSetPixelFormat', 'wglDescribePixelFormat',
    'wglGetPixelFormat',
]

out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src', 'proxy', 'gl_passthrough.cpp')

with open(out_path, 'w') as f:
    f.write("""// AUTO-GENERATED by tools/gen_passthrough.py — do not edit manually
// Passthrough forwarding stubs for OpenGL functions we don't intercept.
// Each function resolves from real opengl32.dll on first call via naked asm trampoline.

#include "gl_proxy.h"

extern "C" {

""")

    for fn in passthrough:
        f.write(f'static FARPROC pfn_{fn} = nullptr;\n')
        f.write(f'__declspec(naked) void __cdecl fwd_{fn}() {{\n')
        f.write(f'    __asm {{\n')
        f.write(f'        mov eax, dword ptr [pfn_{fn}]\n')
        f.write(f'        test eax, eax\n')
        f.write(f'        jnz L_{fn}\n')
        f.write(f'        push {hex(hash(fn) & 0xFFFFFFFF)}\n')
        f.write(f'        pop eax\n')
        # Resolve at init time instead — simpler approach
        f.write(f'        mov eax, dword ptr [pfn_{fn}]\n')
        f.write(f'    L_{fn}:\n')
        f.write(f'        jmp eax\n')
        f.write(f'    }}\n')
        f.write(f'}}\n\n')

    # Init function to resolve all at once
    f.write("""
void glPassthroughInit() {
""")
    for fn in passthrough:
        f.write(f'    pfn_{fn} = GetProcAddress(g_gl.realOpenGL32, "{fn}");\n')

    f.write("""}\n
} // extern "C"
""")

print(f"Generated {out_path} with {len(passthrough)} forwarding stubs")

# Also generate the .def file entries
def_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src', 'proxy', 'gl_exports.def')

# Read existing .def, we'll rewrite it completely
hooked_exports = [
    # Vertex submission
    ('glBegin', 'hooked_glBegin'), ('glEnd', 'hooked_glEnd'),
    ('glVertex3f', 'hooked_glVertex3f'), ('glVertex3fv', 'hooked_glVertex3fv'),
    ('glVertex2f', 'hooked_glVertex2f'),
    ('glNormal3f', 'hooked_glNormal3f'), ('glNormal3fv', 'hooked_glNormal3fv'),
    ('glTexCoord2f', 'hooked_glTexCoord2f'), ('glTexCoord2fv', 'hooked_glTexCoord2fv'),
    ('glColor3f', 'hooked_glColor3f'), ('glColor4f', 'hooked_glColor4f'),
    ('glColor4fv', 'hooked_glColor4fv'),
    ('glColor3ub', 'hooked_glColor3ub'), ('glColor4ub', 'hooked_glColor4ub'),
    ('glColor4ubv', 'hooked_glColor4ubv'),
    # Draw calls
    ('glDrawElements', 'hooked_glDrawElements'), ('glDrawArrays', 'hooked_glDrawArrays'),
    # Textures
    ('glBindTexture', 'hooked_glBindTexture'),
    ('glTexImage2D', 'hooked_glTexImage2D'), ('glTexSubImage2D', 'hooked_glTexSubImage2D'),
    ('glGenTextures', 'hooked_glGenTextures'), ('glDeleteTextures', 'hooked_glDeleteTextures'),
    ('glTexParameteri', 'hooked_glTexParameteri'), ('glTexParameterf', 'hooked_glTexParameterf'),
    # State
    ('glEnable', 'hooked_glEnable'), ('glDisable', 'hooked_glDisable'),
    ('glBlendFunc', 'hooked_glBlendFunc'), ('glDepthFunc', 'hooked_glDepthFunc'),
    ('glDepthMask', 'hooked_glDepthMask'), ('glAlphaFunc', 'hooked_glAlphaFunc'),
    ('glColorMask', 'hooked_glColorMask'),
    ('glStencilFunc', 'hooked_glStencilFunc'), ('glStencilOp', 'hooked_glStencilOp'),
    ('glStencilMask', 'hooked_glStencilMask'),
    ('glCullFace', 'hooked_glCullFace'), ('glPolygonMode', 'hooked_glPolygonMode'),
    ('glPolygonOffset', 'hooked_glPolygonOffset'), ('glScissor', 'hooked_glScissor'),
    # Matrix
    ('glMatrixMode', 'hooked_glMatrixMode'), ('glLoadIdentity', 'hooked_glLoadIdentity'),
    ('glLoadMatrixf', 'hooked_glLoadMatrixf'), ('glMultMatrixf', 'hooked_glMultMatrixf'),
    ('glPushMatrix', 'hooked_glPushMatrix'), ('glPopMatrix', 'hooked_glPopMatrix'),
    ('glOrtho', 'hooked_glOrtho'), ('glFrustum', 'hooked_glFrustum'),
    # Viewport/Clear
    ('glViewport', 'hooked_glViewport'), ('glClear', 'hooked_glClear'),
    ('glClearColor', 'hooked_glClearColor'), ('glClearDepth', 'hooked_glClearDepth'),
    ('glReadBuffer', 'hooked_glReadBuffer'), ('glReadPixels', 'hooked_glReadPixels'),
    ('glFinish', 'hooked_glFinish'), ('glFlush', 'hooked_glFlush'),
    ('glGetError', 'hooked_glGetError'), ('glGetIntegerv', 'hooked_glGetIntegerv'),
    ('glGetFloatv', 'hooked_glGetFloatv'), ('glGetString', 'hooked_glGetString'),
    ('glIsEnabled', 'hooked_glIsEnabled'), ('glPixelStorei', 'hooked_glPixelStorei'),
    ('glHint', 'hooked_glHint'),
    # Client state
    ('glEnableClientState', 'hooked_glEnableClientState'),
    ('glDisableClientState', 'hooked_glDisableClientState'),
    ('glVertexPointer', 'hooked_glVertexPointer'), ('glNormalPointer', 'hooked_glNormalPointer'),
    ('glTexCoordPointer', 'hooked_glTexCoordPointer'), ('glColorPointer', 'hooked_glColorPointer'),
    # Misc
    ('glPushAttrib', 'hooked_glPushAttrib'), ('glPopAttrib', 'hooked_glPopAttrib'),
    ('glLineWidth', 'hooked_glLineWidth'), ('glPointSize', 'hooked_glPointSize'),
    # WGL
    ('wglCreateContext', 'hooked_wglCreateContext'), ('wglDeleteContext', 'hooked_wglDeleteContext'),
    ('wglMakeCurrent', 'hooked_wglMakeCurrent'), ('wglGetProcAddress', 'hooked_wglGetProcAddress'),
    ('wglGetCurrentContext', 'hooked_wglGetCurrentContext'),
    ('wglGetCurrentDC', 'hooked_wglGetCurrentDC'),
    ('wglShareLists', 'hooked_wglShareLists'),
    ('wglSwapBuffers', 'hooked_wglSwapBuffers'), ('wglSwapLayerBuffers', 'hooked_wglSwapLayerBuffers'),
]

with open(def_path, 'w') as f:
    f.write("LIBRARY opengl32\nEXPORTS\n")
    f.write("    ; === HOOKED FUNCTIONS (custom interception logic) ===\n")
    for gl_name, hook_name in hooked_exports:
        f.write(f"    {gl_name} = {hook_name}\n")
    f.write("\n    ; === PASSTHROUGH FUNCTIONS (forwarded to real opengl32.dll) ===\n")
    for fn in passthrough:
        f.write(f"    {fn} = fwd_{fn}\n")

print(f"Generated {def_path} with {len(hooked_exports)} hooked + {len(passthrough)} passthrough exports")
