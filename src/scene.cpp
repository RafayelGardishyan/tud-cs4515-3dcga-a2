//
// Created by Rafayel on 10/11/2025.
//

#include "scene.h"
#include <array>
#include <string>
#include <cmath>
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
DISABLE_WARNINGS_POP()

namespace {
constexpr GLint SHADOW_MAP_TEXTURE_UNIT = 5;
constexpr GLint SHADOW_CUBEMAP_TEXTURE_UNIT = 6;
}

void RS_Scene::draw(const Shader& drawShader, RS_RenderSettings settings)
{
    if (m_cameras.empty()) {
        // No camera to render from
        return;
    }

    // Bind the shader
    drawShader.bind();

    // Get camera matrices
    const Trackball& camera = getActiveCamera();
    const glm::mat4 viewMatrix = camera.viewMatrix();
    const glm::mat4 projectionMatrix = camera.projectionMatrix();
    const glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    // Set global texture toggles
    glUniform1i(drawShader.getUniformLocation("enableColorTextures"), settings.enableColorTextures ? 1 : 0);
    glUniform1i(drawShader.getUniformLocation("enableNormalTextures"), settings.enableNormalTextures ? 1 : 0);
    glUniform1i(drawShader.getUniformLocation("enableMetallicTextures"), settings.enableMetallicTextures ? 1 : 0);
    glUniform1i(drawShader.getUniformLocation("enableGammaCorrection"), settings.enableGammaCorrection ? 1 : 0);
    glUniform1i(drawShader.getUniformLocation("enableToneMapping"), settings.enableToneMapping ? 1 : 0);

    glUniform3fv(drawShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(camera.position()));
    glUniform1i(drawShader.getUniformLocation("enableShadows"), settings.enableShadows ? 1 : 0);
    glUniform1i(drawShader.getUniformLocation("enableShadowPCF"), settings.enableShadowPCF ? 1 : 0);
    glUniform2f(drawShader.getUniformLocation("shadowMapTexelSize"), 1.0f / RS_SHADOW_MAP_SIZE, 1.0f / RS_SHADOW_MAP_SIZE);

    if (!m_lights.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthFunc(GL_EQUAL);
        glDepthMask(GL_FALSE);
    }

    // Multi-pass lighting: render scene once for each light with additive blending
    for (size_t lightIndex = 0; lightIndex < m_lights.size(); lightIndex++) {
        RS_Light& light = m_lights[lightIndex];

        glUniform3fv(drawShader.getUniformLocation("lightPosition"), 1, glm::value_ptr(light.m_position));
        glUniform3fv(drawShader.getUniformLocation("lightColor"), 1, glm::value_ptr(light.m_color));
        glUniform1f(drawShader.getUniformLocation("lightIntensity"), light.m_intensity);
        glUniform1i(drawShader.getUniformLocation("lightType"), static_cast<int>(light.m_type));

        const glm::vec3 lightDir = light.getDirection();

        glUniform3fv(drawShader.getUniformLocation("lightDirection"), 1, glm::value_ptr(lightDir));
        glUniform1f(drawShader.getUniformLocation("spotlightCosCutoff"), glm::cos(light.m_spotFov * 0.5f));
        glUniform1f(drawShader.getUniformLocation("shadowFarPlane"), light.getShadowFarPlane());

        if (settings.enableShadows) {
            if (light.m_type == RS_LIGHT_TYPE_SPOT && light.m_shadowMapTexture) {
                glUniformMatrix4fv(
                    drawShader.getUniformLocation("lightSpaceMatrix"),
                    1,
                    GL_FALSE,
                    glm::value_ptr(light.getLightSpaceMatrix()));

                light.bindShadowMap(GL_TEXTURE0 + SHADOW_MAP_TEXTURE_UNIT);
                glUniform1i(drawShader.getUniformLocation("shadowMap"), SHADOW_MAP_TEXTURE_UNIT);
            } else if (light.m_type == RS_LIGHT_TYPE_POINT && light.m_cubeMapTexture) {
                light.bindCubeMap(GL_TEXTURE0 + SHADOW_CUBEMAP_TEXTURE_UNIT);
                glUniform1i(drawShader.getUniformLocation("shadowCubemap"), SHADOW_CUBEMAP_TEXTURE_UNIT);
            }
        }

        for (RS_Model& model : m_models) {
            model.draw(drawShader, viewProjectionMatrix);
        }
    }

    if (!m_lights.empty()) {
        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
    }
}

void RS_Scene::drawEnvironment(const Shader& envShader, RS_RenderSettings settings)
{
    if (m_cameras.empty() || !m_environmentCubemap) {
        return;
    }

    // Bind the environment shader
    envShader.bind();

    const Trackball& camera = getActiveCamera();
    const glm::mat4 viewMatrix = camera.viewMatrix();
    const glm::mat4 projectionMatrix = camera.projectionMatrix();
    const glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    // Set global texture toggles
    glUniform1i(envShader.getUniformLocation("enableColorTextures"), settings.enableColorTextures ? 1 : 0);
    glUniform1i(envShader.getUniformLocation("enableNormalTextures"), settings.enableNormalTextures ? 1 : 0);
    glUniform1i(envShader.getUniformLocation("enableMetallicTextures"), settings.enableMetallicTextures ? 1 : 0);
    glUniform1i(envShader.getUniformLocation("enableGammaCorrection"), settings.enableGammaCorrection ? 1 : 0);
    glUniform1i(envShader.getUniformLocation("enableToneMapping"), settings.enableToneMapping ? 1 : 0);

    glActiveTexture(GL_TEXTURE0);
    m_environmentCubemap->bind(GL_TEXTURE0);
    glUniform1i(envShader.getUniformLocation("environmentMap"), 0);
    glUniform3fv(envShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(camera.position()));
    glUniform1f(envShader.getUniformLocation("envBrightness"), m_envBrightness);

    // Draw all models with environment shader (writes depth)
    for (RS_Model& model : m_models) {
        model.draw(envShader, viewProjectionMatrix);
    }
}

void RS_Scene::drawSkybox(const Shader& skyboxShader, GLuint skyboxVAO)
{
    if (m_cameras.empty() || !m_environmentCubemap) {
        return;
    }

    skyboxShader.bind();

    const Trackball& camera = getActiveCamera();
    const glm::mat4 viewMatrix = camera.viewMatrix();
    const glm::mat4 projectionMatrix = camera.projectionMatrix();

    glUniformMatrix4fv(skyboxShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
    glUniformMatrix4fv(skyboxShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
    glUniform1f(skyboxShader.getUniformLocation("envBrightness"), m_envBrightness);

    // Bind cubemap
    glActiveTexture(GL_TEXTURE0);
    m_environmentCubemap->bind(GL_TEXTURE0);
    glUniform1i(skyboxShader.getUniformLocation("skybox"), 0);

    // Render skybox at maximum depth
    glDepthFunc(GL_LEQUAL);
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

void RS_Scene::renderShadowMaps(const Shader& shadowShader,
    const Shader& shadowCubemapShader,
    const RS_RenderSettings& settings)
{
    if (!settings.enableShadows || m_lights.empty() || m_models.empty())
        return;

    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT); // Reduce peter-panning

    for (RS_Light& light : m_lights) {
        if (light.m_type == RS_LIGHT_TYPE_SPOT && light.m_shadowMapTexture) {
            glViewport(0, 0, RS_SHADOW_MAP_SIZE, RS_SHADOW_MAP_SIZE);
            glBindFramebuffer(GL_FRAMEBUFFER, light.getShadowFBO());
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D,
                light.m_shadowMapTexture->getTextureID(),
                0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            glClear(GL_DEPTH_BUFFER_BIT);

            shadowShader.bind();

            glm::vec3 direction = light.getDirection();
            glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

            glm::vec3 target = light.getLookAtTarget();
            if (glm::length(target - light.m_position) < 0.001f)
                target = light.m_position + direction;
            const glm::mat4 lightView = glm::lookAt(light.m_position, target, up);
            const glm::mat4 lightProjection = glm::perspective(
                light.m_spotFov,
                1.0f,
                light.getShadowNearPlane(),
                light.getShadowFarPlane());
            const glm::mat4 lightSpaceMatrix = lightProjection * lightView;
            light.setLightSpaceMatrix(lightSpaceMatrix);

            for (RS_Model& model : m_models) {
                model.drawDepth(shadowShader, lightSpaceMatrix);
            }
        } else if (light.m_type == RS_LIGHT_TYPE_POINT && light.m_cubeMapTexture) {
            const int resolution = light.m_cubeMapTexture->getResolution();
            glViewport(0, 0, resolution, resolution);
            glBindFramebuffer(GL_FRAMEBUFFER, light.getShadowCubemapFBO());
            glFramebufferTexture(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                light.m_cubeMapTexture->getCubemapID(),
                0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            glClear(GL_DEPTH_BUFFER_BIT);

            shadowCubemapShader.bind();

            const float nearPlane = light.getShadowNearPlane();
            const float farPlane = light.getShadowFarPlane();
            const glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
            std::array<glm::mat4, 6> shadowTransforms {
                shadowProj * glm::lookAt(light.m_position, light.m_position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                shadowProj * glm::lookAt(light.m_position, light.m_position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                shadowProj * glm::lookAt(light.m_position, light.m_position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                shadowProj * glm::lookAt(light.m_position, light.m_position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                shadowProj * glm::lookAt(light.m_position, light.m_position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                shadowProj * glm::lookAt(light.m_position, light.m_position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
            };
            light.setShadowTransforms(shadowTransforms);

            for (int i = 0; i < 6; ++i) {
                const std::string uniformName = "shadowMatrices[" + std::to_string(i) + "]";
                glUniformMatrix4fv(
                    shadowCubemapShader.getUniformLocation(uniformName.c_str()),
                    1,
                    GL_FALSE,
                    glm::value_ptr(shadowTransforms[i]));
            }

            glUniform3fv(shadowCubemapShader.getUniformLocation("lightPosition"), 1, glm::value_ptr(light.m_position));
            glUniform1f(shadowCubemapShader.getUniformLocation("farPlane"), farPlane);

            for (RS_Model& model : m_models) {
                model.drawDepthCubemap(shadowCubemapShader);
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glCullFace(GL_BACK);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glDepthMask(GL_TRUE);
}
