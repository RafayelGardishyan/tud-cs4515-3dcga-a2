//
// Created by Rafayel on 10/11/2025.
//

#include "scene.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_POP()

RS_Scene::RS_Scene()
{
    m_lightShader = ShaderBuilder()
        .addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/light_vert.glsl")
        .addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/light_frag.glsl")
        .build();

    // Create VAO for light visualization (no VBO needed - drawing points)
    glGenVertexArrays(1, &m_lightVAO);
}

RS_Scene::~RS_Scene()
{
    // Clean up light VAO
    if (m_lightVAO != 0) {
        glDeleteVertexArrays(1, &m_lightVAO);
    }
}

// Move constructor
RS_Scene::RS_Scene(RS_Scene&& other) noexcept
    : m_models(std::move(other.m_models)),
      m_lights(std::move(other.m_lights)),
      m_selectedLightIndex(other.m_selectedLightIndex),
      m_lightShader(std::move(other.m_lightShader)),
      m_lightVAO(other.m_lightVAO),
      m_cameras(std::move(other.m_cameras)),
      m_activeCameraIndex(other.m_activeCameraIndex)
{
    // Transfer ownership - prevent other from deleting the VAO
    other.m_lightVAO = 0;
}

// Move assignment operator
RS_Scene& RS_Scene::operator=(RS_Scene&& other) noexcept
{
    if (this != &other) {
        // Clean up this object's resources
        if (m_lightVAO != 0) {
            glDeleteVertexArrays(1, &m_lightVAO);
        }

        // Transfer ownership from other
        m_models = std::move(other.m_models);
        m_lights = std::move(other.m_lights);
        m_selectedLightIndex = other.m_selectedLightIndex;
        m_lightShader = std::move(other.m_lightShader);
        m_lightVAO = other.m_lightVAO;
        m_cameras = std::move(other.m_cameras);
        m_activeCameraIndex = other.m_activeCameraIndex;

        // Prevent other from deleting the VAO
        other.m_lightVAO = 0;
    }
    return *this;
}

void RS_Scene::draw(const Shader& drawShader, bool debugLights)
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
        glUniform1f(drawShader.getUniformLocation("lightIntensity"), m_lights[lightIndex].intensity);

        // Draw all models
        for (RS_Model& model : m_models) {
            model.draw(drawShader, viewProjectionMatrix);
        }
    }

    // Reset OpenGL state
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);

    // Render the lights is debug is enabled:
    m_lightShader.bind();
    {
        const glm::vec4 screenPos = viewProjectionMatrix * glm::vec4(m_lights[m_selectedLightIndex].position, 1.0f);
        const glm::vec3 color{1, 1, 0};

        glPointSize(40.0f);
        glUniform4fv(m_lightShader.getUniformLocation("pos"), 1, glm::value_ptr(screenPos));
        glUniform3fv(m_lightShader.getUniformLocation("color"), 1, glm::value_ptr(color));
        glBindVertexArray(m_lightVAO);
        glDrawArrays(GL_POINTS, 0, 1);
        glBindVertexArray(0);
    }
    for (const RS_Light& light : m_lights)
    {
        const glm::vec4 screenPos = viewProjectionMatrix * glm::vec4(light.position, 1.0f);

        glPointSize(10.0f);
        glUniform4fv(m_lightShader.getUniformLocation("pos"), 1, glm::value_ptr(screenPos));
        glUniform3fv(m_lightShader.getUniformLocation("color"), 1, glm::value_ptr(light.color));
        glBindVertexArray(m_lightVAO);
        glDrawArrays(GL_POINTS, 0, 1);
        glBindVertexArray(0);
    }

}
