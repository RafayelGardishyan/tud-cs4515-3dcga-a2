//
// Created by Rafayel on 10/14/2025.
//

#include "light.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_POP()
#include <iostream>

RS_Light::RS_Light(glm::vec3 position, glm::vec3 color, float intensity, RS_LightType type)
    : m_position(position)
      , m_color(color)
      , m_intensity(intensity)
      , m_type(type)
      , m_spotFov(glm::radians(45.0f))
      , m_direction(glm::vec3(0.0f, -1.0f, 0.0f))
{
    // Create depth cubemap for omnidirectional shadow mapping
    m_cubeMapTexture = RS_Cubemap::createDepthCubemap(RS_CUBEMAP_SIZE);

    // Create depth texture for directional/spot shadow mapping
    m_shadowMapTexture = RS_Texture::createDepthTexture(RS_SHADOW_MAP_SIZE, RS_SHADOW_MAP_SIZE);
}
