#include "renderer/light.h"

Light::Light(float x, float y, float z,
             float r, float g, float b,
             float radius, float intensity)
    : m_radius(radius), m_intensity(intensity) {
    m_position[0] = x; m_position[1] = y; m_position[2] = z;
    m_color[0] = r; m_color[1] = g; m_color[2] = b;
}

void Light::SetPosition(float x, float y, float z) {
    m_position[0] = x; m_position[1] = y; m_position[2] = z;
}

void Light::SetColor(float r, float g, float b) {
    m_color[0] = r; m_color[1] = g; m_color[2] = b;
}
