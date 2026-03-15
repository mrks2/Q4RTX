#include "renderer/window.h"
#include "renderer/renderer.h"
#include "renderer/frame_receiver.h"
#include <cstdio>

static constexpr uint32_t WIDTH  = 1280;
static constexpr uint32_t HEIGHT = 720;

int main() {
    FILE* logFile = freopen("q4rtx_renderer.log", "w", stdout);
    setvbuf(stdout, nullptr, _IONBF, 0);

    HANDLE hMutex = CreateMutexA(nullptr, FALSE, "Q4RTX_RendererRunning");
    printf("Q4RTX Renderer starting...\n");

    FrameReceiver receiver;
    if (!receiver.Connect()) return 1;

    Window window;
    if (!window.Create("Q4RTX - Ray Traced", WIDTH, HEIGHT)) {
        printf("ERROR: Failed to create window\n");
        return 1;
    }

    Renderer renderer;
    if (!renderer.Init(window.GetHwnd(), WIDTH, HEIGHT)) {
        printf("ERROR: Failed to init renderer\n");
        return 1;
    }
    printf("Renderer initialized\n");

    uint32_t updateCount = 0;

    while (window.PumpMessages()) {
        const SharedFrameHeader* frame;
        const SharedMeshHeader* meshHeaders;
        const SharedVertex* vertices;
        const uint32_t* indices;
        const SharedLight* lights;

        if (receiver.TryReceive(frame, meshHeaders, vertices, indices, lights)) {
            printf(">> Frame %u: meshes=%u verts=%u idx=%u lights=%u\n",
                   frame->frame_number, frame->mesh_count,
                   frame->total_vertices, frame->total_indices, frame->light_count);

            renderer.UpdateScene(*frame, meshHeaders, vertices, indices, lights);
            receiver.Acknowledge();
            updateCount++;

            printf("<< Update #%u done\n", updateCount);
        }

        renderer.RenderFrame();
    }

    renderer.Shutdown();
    receiver.Shutdown();

    return 0;
}
