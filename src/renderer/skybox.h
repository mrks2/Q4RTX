#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

// Must match HLSL SkyboxConstants
struct SkyboxConstants {
    float zenithColor[4];
    float horizonColor[4];
    float groundColor[4];
    float sunDir[4];
    float sunColor[4];
    float sunSize;
    float sunFalloff;
    float atmosphereDensity;
    float pad;
};

class Skybox {
public:
    void SetZenithColor(float r, float g, float b);
    void SetHorizonColor(float r, float g, float b);
    void SetGroundColor(float r, float g, float b);
    void SetSunDirection(float x, float y, float z);
    void SetSunColor(float r, float g, float b);
    void SetSunSize(float cosAngle);
    void SetSunFalloff(float falloff);
    void SetAtmosphereDensity(float density);

    void Upload(ID3D12Device* device);

    ID3D12Resource* GetConstantBuffer() { return m_constantBuffer.Get(); }

private:
    SkyboxConstants m_params = {
        { 0.15f, 0.3f, 0.8f, 0.0f },   // zenith: deep blue
        { 0.6f, 0.7f, 0.9f, 0.0f },     // horizon: pale blue
        { 0.08f, 0.08f, 0.06f, 0.0f },  // ground
        { 0.4f, 0.3f, 0.8f, 0.0f },     // sun direction
        { 1.5f, 1.3f, 1.0f, 0.0f },     // sun color (>1 for bloom)
        0.9998f,                          // sun size
        64.0f,                            // sun falloff
        3.0f,                             // atmosphere density
        0.0f
    };

    ComPtr<ID3D12Resource> m_constantBuffer;
};
