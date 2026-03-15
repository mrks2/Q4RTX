#pragma once
// Shared state between gl_hooks_*.cpp files.

#include "gl_state.h"
#include "vbo_tracker.h"
#include <cstdint>

extern TrackedGLState g_st;
extern VBOTracker     g_vboTracker;

extern float g_viewMatrix[16];
extern float g_projMatrix[16];
extern bool  g_viewCaptured;

// Light state (set by program param hooks, read by draw hooks)
extern float    g_curLightOriginLocal[4];
extern float    g_curLightColor[4];
extern bool     g_lightOriginSet;
extern bool     g_lightColorSet;

// Debug counters
extern uint32_t g_drawCallsThisFrame;
extern uint32_t g_capturedThisFrame;
extern uint32_t g_failDepth, g_failDW, g_failOrtho, g_failMode, g_failCount, g_passFilter;
extern uint32_t g_envParamCalls, g_localParamCalls, g_lightCaptureAttempts;
