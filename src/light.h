#pragma once

#include <framework/disable_all_warnings.h>

#include "glad/glad.h"
#include "cubemap.h"
#include "texture.h"
#include <optional>
#include <array>
DISABLE_WARNINGS_PUSH()
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
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

    RS_Light(const RS_Light&) = delete;
    RS_Light& operator=(const RS_Light&) = delete;
    RS_Light(RS_Light&& other) noexcept;
    RS_Light& operator=(RS_Light&& other) noexcept;
    ~RS_Light();

    void bindShadowMap(GLint textureSlot) const;
    void bindCubeMap(GLint textureSlot) const;

    GLuint getShadowFBO() const { return m_shadowFBO; }
    GLuint getShadowCubemapFBO() const { return m_shadowCubemapFBO; }

    void setLightSpaceMatrix(const glm::mat4& matrix) { m_lightSpaceMatrix = matrix; }
    const glm::mat4& getLightSpaceMatrix() const { return m_lightSpaceMatrix; }

    void setShadowTransforms(const std::array<glm::mat4, 6>& transforms) { m_shadowTransforms = transforms; }
    const std::array<glm::mat4, 6>& getShadowTransforms() const { return m_shadowTransforms; }

    float getShadowNearPlane() const { return m_shadowNearPlane; }
    float getShadowFarPlane() const { return m_shadowFarPlane; }
    void setShadowNearPlane(float nearPlane);
    void setShadowFarPlane(float farPlane);
    void setShadowRange(float nearPlane, float farPlane);

    float getSpotFov() const { return m_spotFov; }
    void setSpotFov(float radians);

    glm::vec3 getLookAtTarget() const { return m_target; }
    void setLookAtTarget(const glm::vec3& target);
    glm::vec3 getDirection() const;
    void setDirection(const glm::vec3& direction);

    glm::vec3 m_position { 0.0f, 2.0f, 0.0f };
    glm::vec3 m_color { 1.0f, 1.0f, 1.0f };
    float m_intensity { 1.0f };
    float m_spotFov;

    RS_LightType m_type;
    std::optional<RS_Texture> m_shadowMapTexture;
    std::optional<RS_Cubemap> m_cubeMapTexture;

private:
    void initializeShadowResources();
    void destroyShadowResources();

    GLuint m_shadowFBO { 0 };
    GLuint m_shadowCubemapFBO { 0 };
    glm::mat4 m_lightSpaceMatrix { 1.0f };
    std::array<glm::mat4, 6> m_shadowTransforms{};
    float m_shadowNearPlane { 0.1f };
    float m_shadowFarPlane { 50.0f };
    glm::vec3 m_target { 0.0f, 1.0f, 0.0f };
};
