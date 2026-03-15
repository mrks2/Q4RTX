#include "renderer/frame_receiver.h"
#include <cstdio>

bool FrameReceiver::Connect() {
    // Wait for shared memory to appear (proxy may not be up yet)
    while (!m_shmHandle) {
        m_shmHandle = OpenOrCreateSharedMem(&m_shmPtr, false);
        if (!m_shmHandle) Sleep(500);
    }
    printf("Connected to geometry shared memory\n");

    m_evtReady = OpenEventA(SYNCHRONIZE, FALSE, EVT_DATA_READY);
    m_evtRead  = OpenEventA(EVENT_MODIFY_STATE, FALSE, EVT_DATA_READ);
    if (!m_evtReady || !m_evtRead) {
        printf("ERROR: Could not open geometry sync events\n");
        return false;
    }

    return true;
}

void FrameReceiver::Shutdown() {
    if (m_shmPtr)    { UnmapViewOfFile(m_shmPtr); m_shmPtr = nullptr; }
    if (m_shmHandle) { CloseHandle(m_shmHandle); m_shmHandle = nullptr; }
    if (m_evtReady)  { CloseHandle(m_evtReady); m_evtReady = nullptr; }
    if (m_evtRead)   { CloseHandle(m_evtRead); m_evtRead = nullptr; }
}

bool FrameReceiver::TryReceive(const SharedFrameHeader*& frame,
                                const SharedMeshHeader*& meshHeaders,
                                const SharedVertex*& vertices,
                                const uint32_t*& indices,
                                const SharedLight*& lights) {
    if (WaitForSingleObject(m_evtReady, 0) != WAIT_OBJECT_0)
        return false;

    // Auto-reset consumed evt_ready -- must call Acknowledge() or proxy blocks
    uint8_t* base = (uint8_t*)m_shmPtr;
    frame = (const SharedFrameHeader*)(base + OFFSET_FRAME_HEADER);

    if (frame->frame_number == m_lastFrame || frame->mesh_count == 0) {
        Acknowledge();
        return false;
    }

    meshHeaders = (const SharedMeshHeader*)(base + OFFSET_MESH_HEADERS);
    vertices    = (const SharedVertex*)(base + OFFSET_VERTEX_DATA);
    indices     = (const uint32_t*)(base + OFFSET_INDEX_DATA);
    lights      = (const SharedLight*)(base + OFFSET_LIGHT_DATA);

    m_lastFrame = frame->frame_number;
    return true;
}

void FrameReceiver::Acknowledge() {
    SetEvent(m_evtRead);
}
