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
        // Enable additive blending for lights after the first one
        if (lightIndex == 0) {
            glDisable(GL_BLEND);
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE); // Additive blending
            glDepthFunc(GL_EQUAL);       // Only render fragments at the same depth
        }

        glUniform3fv(drawShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(camera.position()));
        glUniform3fv(drawShader.getUniformLocation("lightPosition"), 1, glm::value_ptr(m_lights[lightIndex].position));
        glUniform3fv(drawShader.getUniformLocation("lightColor"), 1, glm::value_ptr(m_lights[lightIndex].color));
        glUniform3fv(drawShader.getUniformLocation("lightIntensity"), 1, &m_lights[lightIndex].intensity);

        // Draw all models
        for (RS_Model& model : m_models) {
            model.draw(drawShader, viewProjectionMatrix);
        }
    }

    // Reset OpenGL state
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
}