//
// Created by Rafayel on 10/11/2025.
//

#include "scene.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_POP()

void RS_Scene::draw(const Shader& drawShader)
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

    // Multi-pass lighting: render scene once for each light with additive blending
    for (size_t lightIndex = 0; lightIndex < m_lights.size(); lightIndex++) {
        // Enable additive blending for all lights
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthFunc(GL_EQUAL);

        glUniform3fv(drawShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(camera.position()));
        glUniform3fv(drawShader.getUniformLocation("lightPosition"), 1, glm::value_ptr(m_lights[lightIndex].position));
        glUniform3fv(drawShader.getUniformLocation("lightColor"), 1, glm::value_ptr(m_lights[lightIndex].color));
        glUniform1f(drawShader.getUniformLocation("lightIntensity"), m_lights[lightIndex].intensity);

        for (RS_Model& model : m_models) {
            model.draw(drawShader, viewProjectionMatrix);
        }
    }

    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
}

void RS_Scene::drawEnvironment(const Shader& envShader)
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
