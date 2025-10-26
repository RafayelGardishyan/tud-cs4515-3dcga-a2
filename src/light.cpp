//
// Created by Rafayel on 10/14/2025.
//

#include "light.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_POP()
#include <algorithm>
#include <iostream>

RS_Light::RS_Light(glm::vec3 position, glm::vec3 color, float intensity, RS_LightType type)
    : m_position(position)
    , m_color(color)
    , m_intensity(intensity)
    , m_type(type)
    , m_spotFov(glm::radians(45.0f))
{
    // Create depth resources for both shadowing strategies so we can switch light type dynamically.
    m_cubeMapTexture = RS_Cubemap::createDepthCubemap(RS_CUBEMAP_SIZE);
    m_shadowMapTexture = RS_Texture::createDepthTexture(RS_SHADOW_MAP_SIZE, RS_SHADOW_MAP_SIZE);
    m_target = m_position + glm::vec3(0.0f, -1.0f, 0.0f);
    initializeShadowResources();
}

RS_Light::RS_Light(RS_Light&& other) noexcept
{
    *this = std::move(other);
}

RS_Light& RS_Light::operator=(RS_Light&& other) noexcept
{
    if (this == &other)
        return *this;

    destroyShadowResources();
    m_position = other.m_position;
    m_color = other.m_color;
    m_intensity = other.m_intensity;
    m_spotFov = other.m_spotFov;
    m_type = other.m_type;
    m_shadowNearPlane = other.m_shadowNearPlane;
    m_shadowFarPlane = other.m_shadowFarPlane;
    m_lightSpaceMatrix = other.m_lightSpaceMatrix;
    m_shadowTransforms = other.m_shadowTransforms;
    m_target = other.m_target;
    m_shadowMapTexture = std::move(other.m_shadowMapTexture);
    m_cubeMapTexture = std::move(other.m_cubeMapTexture);
    m_shadowFBO = other.m_shadowFBO;
    m_shadowCubemapFBO = other.m_shadowCubemapFBO;

    other.m_shadowFBO = 0;
    other.m_shadowCubemapFBO = 0;

    return *this;
}

RS_Light::~RS_Light()
{
    destroyShadowResources();
}

void RS_Light::bindShadowMap(GLint textureSlot) const
{
    if (m_shadowMapTexture)
        m_shadowMapTexture->bind(textureSlot);
}

void RS_Light::bindCubeMap(GLint textureSlot) const
{
    if (m_cubeMapTexture)
        m_cubeMapTexture->bind(textureSlot);
}

void RS_Light::initializeShadowResources()
{
    destroyShadowResources();

    glGenFramebuffers(1, &m_shadowFBO);
    glGenFramebuffers(1, &m_shadowCubemapFBO);

    if (m_shadowMapTexture) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D,
            m_shadowMapTexture->getTextureID(),
            0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    if (m_cubeMapTexture) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowCubemapFBO);
        glFramebufferTexture(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            m_cubeMapTexture->getCubemapID(),
            0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RS_Light::destroyShadowResources()
{
    if (m_shadowFBO != 0) {
        glDeleteFramebuffers(1, &m_shadowFBO);
        m_shadowFBO = 0;
    }

    if (m_shadowCubemapFBO != 0) {
        glDeleteFramebuffers(1, &m_shadowCubemapFBO);
        m_shadowCubemapFBO = 0;
    }
}

void RS_Light::setShadowNearPlane(float nearPlane)
{
    m_shadowNearPlane = std::max(0.01f, nearPlane);
    if (m_shadowFarPlane <= m_shadowNearPlane + 0.05f)
        m_shadowFarPlane = m_shadowNearPlane + 0.05f;
}

void RS_Light::setShadowFarPlane(float farPlane)
{
    m_shadowFarPlane = std::max(m_shadowNearPlane + 0.05f, farPlane);
}

void RS_Light::setShadowRange(float nearPlane, float farPlane)
{
    setShadowNearPlane(nearPlane);
    setShadowFarPlane(farPlane);
}

void RS_Light::setSpotFov(float radians)
{
    const float minFov = glm::radians(5.0f);
    const float maxFov = glm::radians(170.0f);
    m_spotFov = std::clamp(radians, minFov, maxFov);
}

void RS_Light::setLookAtTarget(const glm::vec3& target)
{
    if (glm::length(target - m_position) < 0.001f)
        m_target = m_position + glm::vec3(0.0f, -1.0f, 0.0f);
    else
        m_target = target;
}

glm::vec3 RS_Light::getDirection() const
{
    glm::vec3 dir = m_target - m_position;
    if (glm::length(dir) < 0.001f)
        dir = glm::vec3(0.0f, -1.0f, 0.0f);
    return glm::normalize(dir);
}

void RS_Light::setDirection(const glm::vec3& direction)
{
    if (glm::length(direction) < 0.001f)
        return;
    m_target = m_position + glm::normalize(direction);
}
