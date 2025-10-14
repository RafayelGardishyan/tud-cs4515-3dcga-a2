#pragma once

#include <framework/disable_all_warnings.h>

#include "glad/glad.h"
#include "cubemap.h"
#include "texture.h"
#include <optional>
DISABLE_WARNINGS_PUSH()
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()

enum RS_LightType
{
    RS_LIGHT_TYPE_POINT = 0,
    RS_LIGHT_TYPE_SPOT = 1
};

const uint32_t RS_SHADOW_MAP_SIZE = 1024;
const uint32_t RS_CUBEMAP_SIZE = 512;

// Basic light structure - expand with additional properties as needed
class RS_Light {
public:
    RS_Light(glm::vec3 position, glm::vec3 color, float intensity, RS_LightType type = RS_LIGHT_TYPE_POINT);

    RS_Light()
        : RS_Light(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f) {}

    void getViewMatrix();
    void bindShadowMap();
    void bindCubeMap();

    glm::vec3 m_position { 0.0f, 2.0f, 0.0f };
    glm::vec3 m_color { 1.0f, 1.0f, 1.0f };
    float m_intensity { 1.0f };

    RS_LightType m_type;
    std::optional<RS_Texture> m_shadowMapTexture;
    std::optional<RS_Cubemap> m_cubeMapTexture;
    float m_spotFov;
    glm::vec3 m_direction;
};