#include "renderer/skybox.h"
#include "renderer/dxr_helpers.h"
#include <cmath>

void Skybox::SetZenithColor(float r, float g, float b) {
    m_params.zenithColor[0] = r; m_params.zenithColor[1] = g; m_params.zenithColor[2] = b;
}

void Skybox::SetHorizonColor(float r, float g, float b) {
    m_params.horizonColor[0] = r; m_params.horizonColor[1] = g; m_params.horizonColor[2] = b;
}

void Skybox::SetGroundColor(float r, float g, float b) {
    m_params.groundColor[0] = r; m_params.groundColor[1] = g; m_params.groundColor[2] = b;
}

void Skybox::SetSunDirection(float x, float y, float z) {
    float len = sqrtf(x*x + y*y + z*z);
    if (len > 0.0001f) {
        m_params.sunDir[0] = x / len;
        m_params.sunDir[1] = y / len;
        m_params.sunDir[2] = z / len;
    }
}

void Skybox::SetSunColor(float r, float g, float b) {
    m_params.sunColor[0] = r; m_params.sunColor[1] = g; m_params.sunColor[2] = b;
}

void Skybox::SetSunSize(float cosAngle) { m_params.sunSize = cosAngle; }
void Skybox::SetSunFalloff(float falloff) { m_params.sunFalloff = falloff; }
void Skybox::SetAtmosphereDensity(float density) { m_params.atmosphereDensity = density; }

void Skybox::Upload(ID3D12Device* device) {
    m_constantBuffer = CreateUploadBuffer(device, &m_params, Align(sizeof(m_params), 256));
}
