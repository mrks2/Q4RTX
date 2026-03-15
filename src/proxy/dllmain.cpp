#include "gl_proxy.h"
#include "frame_sender.h"
#include <cstdio>

GLProxyGlobals g_gl;

#define RESOLVE(name) g_gl.real_##name = \
    reinterpret_cast<decltype(g_gl.real_##name)>(GetProcAddress(g_gl.realOpenGL32, #name))

static bool InitProxy() {
    // load the real system opengl32.dll, not ourselves
    char sysdir[MAX_PATH];
    GetSystemDirectoryA(sysdir, MAX_PATH);
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s\\opengl32.dll", sysdir);

    g_gl.realOpenGL32 = LoadLibraryA(path);
    if (!g_gl.realOpenGL32) {
        MessageBoxA(nullptr, "Q4RTX: Failed to load real opengl32.dll", "Error", MB_OK);
        return false;
    }

    RESOLVE(wglCreateContext); RESOLVE(wglDeleteContext);
    RESOLVE(wglMakeCurrent); RESOLVE(wglGetProcAddress);
    RESOLVE(wglGetCurrentContext); RESOLVE(wglGetCurrentDC);
    RESOLVE(wglShareLists); RESOLVE(wglSwapBuffers); RESOLVE(wglSwapLayerBuffers);

    RESOLVE(glDrawElements); RESOLVE(glDrawArrays);

    RESOLVE(glBegin); RESOLVE(glEnd);
    RESOLVE(glVertex3f); RESOLVE(glVertex3fv); RESOLVE(glVertex2f);
    RESOLVE(glNormal3f); RESOLVE(glNormal3fv);
    RESOLVE(glTexCoord2f); RESOLVE(glTexCoord2fv);
    RESOLVE(glColor3f); RESOLVE(glColor4f); RESOLVE(glColor4fv);
    RESOLVE(glColor3ub); RESOLVE(glColor4ub); RESOLVE(glColor4ubv);

    RESOLVE(glBindTexture); RESOLVE(glTexImage2D); RESOLVE(glTexSubImage2D);
    RESOLVE(glGenTextures); RESOLVE(glDeleteTextures);
    RESOLVE(glTexParameteri); RESOLVE(glTexParameterf);

    RESOLVE(glEnable); RESOLVE(glDisable);
    RESOLVE(glBlendFunc); RESOLVE(glDepthFunc); RESOLVE(glDepthMask);
    RESOLVE(glAlphaFunc); RESOLVE(glColorMask);
    RESOLVE(glStencilFunc); RESOLVE(glStencilOp); RESOLVE(glStencilMask);
    RESOLVE(glCullFace); RESOLVE(glPolygonMode); RESOLVE(glPolygonOffset);
    RESOLVE(glScissor); RESOLVE(glDepthRange);

    RESOLVE(glMatrixMode); RESOLVE(glLoadIdentity); RESOLVE(glLoadMatrixf);
    RESOLVE(glMultMatrixf); RESOLVE(glPushMatrix); RESOLVE(glPopMatrix);
    RESOLVE(glOrtho); RESOLVE(glFrustum);

    RESOLVE(glViewport); RESOLVE(glClear); RESOLVE(glClearColor); RESOLVE(glClearDepth);
    RESOLVE(glReadBuffer); RESOLVE(glReadPixels);
    RESOLVE(glFinish); RESOLVE(glFlush);
    RESOLVE(glGetError); RESOLVE(glGetIntegerv); RESOLVE(glGetFloatv);
    RESOLVE(glGetString); RESOLVE(glIsEnabled); RESOLVE(glPixelStorei); RESOLVE(glHint);

    RESOLVE(glEnableClientState); RESOLVE(glDisableClientState);
    RESOLVE(glVertexPointer); RESOLVE(glNormalPointer);
    RESOLVE(glTexCoordPointer); RESOLVE(glColorPointer);

    RESOLVE(glPushAttrib); RESOLVE(glPopAttrib);
    RESOLVE(glLineWidth); RESOLVE(glPointSize);

    glPassthroughInit();
    frame_sender::Init();

    return true;
}

static PROCESS_INFORMATION g_rendererProc = {};

static void LaunchRenderer() {
    char dllDir[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&LaunchRenderer, &hSelf);
    GetModuleFileNameA(hSelf, dllDir, MAX_PATH);

    char* lastSlash = strrchr(dllDir, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';

    char exePath[MAX_PATH];
    snprintf(exePath, MAX_PATH, "%sq4rtx_renderer.exe", dllDir);

    // don't launch if already running
    HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, "Q4RTX_RendererRunning");
    if (hMutex) {
        CloseHandle(hMutex);
        return;
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    CreateProcessA(exePath, nullptr, nullptr, nullptr, FALSE,
                   0, nullptr, dllDir, &si, &g_rendererProc);
}

static void ShutdownProxy() {
    frame_sender::Shutdown();

    if (g_rendererProc.hProcess) {
        CloseHandle(g_rendererProc.hProcess);
        CloseHandle(g_rendererProc.hThread);
    }

    if (g_gl.realOpenGL32) {
        FreeLibrary(g_gl.realOpenGL32);
        g_gl.realOpenGL32 = nullptr;
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            if (!InitProxy()) return FALSE;
            LaunchRenderer();
            break;
        case DLL_PROCESS_DETACH:
            ShutdownProxy();
            break;
    }
    return TRUE;
}
