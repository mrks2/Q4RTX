// Draw call hooks + frame boundary.

#include "gl_proxy.h"
#include "gl_hooks_internal.h"
#include "frame_sender.h"
#include <gl/GL.h>
#include <cstdio>

uint32_t g_drawCallsThisFrame    = 0;
uint32_t g_failDepth = 0, g_failDW = 0, g_failOrtho = 0;
uint32_t g_failMode = 0, g_failCount = 0, g_passFilter = 0;
uint32_t g_lightCaptureAttempts  = 0;

static uint32_t g_diagSampled = 0;
static FILE*    g_log = nullptr;

static void LogInit() {
    if (!g_log) {
        g_log = fopen("q4rtx_debug.log", "w");
        if (g_log) fprintf(g_log, "Q4RTX proxy debug log\n");
    }
}

// Detect interaction passes (depth_test ON, depth_write OFF, blend ON)
// and forward light params to frame_sender.
static void TryCaptureLight() {
    if (g_st.depth_test && !g_st.depth_write && !g_st.in_ortho && g_st.blend
        && g_lightOriginSet && g_lightColorSet) {
        g_lightCaptureAttempts++;
        frame_sender::AddLight(g_curLightOriginLocal, g_curLightColor, g_st.modelview);
    }
}

extern "C" {

void APIENTRY hooked_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    g_drawCallsThisFrame++;

    if (!g_st.depth_test)          g_failDepth++;
    else if (!g_st.depth_write)    g_failDW++;
    else if (g_st.in_ortho)        g_failOrtho++;
    else if (mode != GL_TRIANGLES) g_failMode++;
    else if (count < 6)            g_failCount++;
    else                           g_passFilter++;

    TryCaptureLight();

    frame_sender::OnDrawElements(mode, count, type, indices, g_st, g_st.modelview);
    g_gl.real_glDrawElements(mode, count, type, indices);
}

void APIENTRY hooked_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    g_drawCallsThisFrame++;

    if (!g_st.depth_test)          g_failDepth++;
    else if (!g_st.depth_write)    g_failDW++;
    else if (g_st.in_ortho)        g_failOrtho++;
    else if (mode != GL_TRIANGLES) g_failMode++;
    else if (count < 6)            g_failCount++;
    else                           g_passFilter++;

    TryCaptureLight();

    frame_sender::OnDrawArrays(mode, first, count, g_st, g_st.modelview);
    g_gl.real_glDrawArrays(mode, first, count);
}

BOOL WINAPI hooked_wglSwapBuffers(HDC hdc) {
    frame_sender::FlushFrame(g_st.frame, g_viewMatrix, g_projMatrix);
    g_viewCaptured = false;

    LogInit();
    if (g_log && (g_st.frame % 60 == 0)) {
        fprintf(g_log, "Frame %u: %u draws, %u captured | fail: depth=%u dw=%u ortho=%u mode=%u cnt=%u pass=%u | vsz=%d vstr=%d vtype=0x%X\n",
                g_st.frame, g_drawCallsThisFrame, g_capturedThisFrame,
                g_failDepth, g_failDW, g_failOrtho, g_failMode, g_failCount, g_passFilter,
                g_st.vert_size, g_st.vert_stride, g_st.vert_type);
        fprintf(g_log, "  LIGHTS: envCalls=%u localCalls=%u captureAttempts=%u originSet=%d colorSet=%d\n",
                g_envParamCalls, g_localParamCalls, g_lightCaptureAttempts,
                (int)g_lightOriginSet, (int)g_lightColorSet);
        fprintf(g_log, "  LightOrigin: %.1f %.1f %.1f  LightColor: %.3f %.3f %.3f\n",
                g_curLightOriginLocal[0], g_curLightOriginLocal[1], g_curLightOriginLocal[2],
                g_curLightColor[0], g_curLightColor[1], g_curLightColor[2]);
        fprintf(g_log, "  ViewMat[12-15]: %.3f %.3f %.3f %.3f\n", g_viewMatrix[12], g_viewMatrix[13], g_viewMatrix[14], g_viewMatrix[15]);
        fprintf(g_log, "  ProjMat[0,5,10,14]: %.3f %.3f %.3f %.3f\n", g_projMatrix[0], g_projMatrix[5], g_projMatrix[10], g_projMatrix[14]);
        fflush(g_log);
    }

    g_drawCallsThisFrame = 0;
    g_capturedThisFrame = 0;
    g_diagSampled = 0;
    g_failDepth = g_failDW = g_failOrtho = g_failMode = g_failCount = g_passFilter = 0;
    g_envParamCalls = 0;
    g_localParamCalls = 0;
    g_lightCaptureAttempts = 0;
    g_lightOriginSet = false;
    g_lightColorSet = false;
    g_st.frame++;
    g_st.in_ortho = false;
    return g_gl.real_wglSwapBuffers(hdc);
}

} // extern "C"
