#pragma once

class Light {
public:
    Light() = default;
    Light(float x, float y, float z,
          float r, float g, float b,
          float radius = 500.0f, float intensity = 1.0f);

    void SetPosition(float x, float y, float z);
    void SetColor(float r, float g, float b);
    void SetRadius(float radius)     { m_radius = radius; }
    void SetIntensity(float intensity) { m_intensity = intensity; }

    const float* GetPosition()  const { return m_position; }
    const float* GetColor()     const { return m_color; }
    float GetRadius()    const { return m_radius; }
    float GetIntensity() const { return m_intensity; }

private:
    float m_position[3]  = {};
    float m_color[3]     = {1.0f, 1.0f, 1.0f};
    float m_radius       = 500.0f;
    float m_intensity    = 1.0f;
};
