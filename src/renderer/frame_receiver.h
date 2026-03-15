#pragma once
#include "shared/shared_memory.h"

class FrameReceiver {
public:
    bool Connect();
    void Shutdown();

    // Non-blocking. Returns true if a new frame is available.
    bool TryReceive(const SharedFrameHeader*& frame,
                    const SharedMeshHeader*& meshHeaders,
                    const SharedVertex*& vertices,
                    const uint32_t*& indices,
                    const SharedLight*& lights);

    void Acknowledge();

private:
    HANDLE   m_shmHandle = nullptr;
    void*    m_shmPtr    = nullptr;
    HANDLE   m_evtReady  = nullptr;
    HANDLE   m_evtRead   = nullptr;
    uint32_t m_lastFrame = UINT32_MAX;
};
