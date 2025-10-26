#include "cubemap.h"
#include "texture.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <iostream>
#include "constants.h"

RS_Cubemap::RS_Cubemap(const RS_Texture& equirectTexture, int resolution)
    : m_resolution(resolution)
{
    // Create the cubemap texture
    glGenTextures(1, &m_cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemap);

    // Allocate storage for all 6 faces
    for (unsigned int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     m_resolution, m_resolution, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    // Set cubemap parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Convert equirectangular to cubemap
    convertEquirectToCubemap(equirectTexture);

    std::cout << "Created cubemap with resolution: " << m_resolution << "x" << m_resolution << std::endl;
}

// Private constructor for creating empty cubemaps
RS_Cubemap::RS_Cubemap(int resolution, bool isDepth)
    : m_resolution(resolution)
{
    glGenTextures(1, &m_cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemap);

    for (unsigned int i = 0; i < 6; i++) {
        if (isDepth) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                         m_resolution, m_resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
}

RS_Cubemap RS_Cubemap::createDepthCubemap(int resolution)
{
    return RS_Cubemap(resolution, true);
}

void RS_Cubemap::convertEquirectToCubemap(const RS_Texture& equirectTexture)
{
    // Save current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    ShaderBuilder builder;
    builder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/equirect_to_cube_vert.glsl");
    builder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/equirect_to_cube_frag.glsl");
    Shader conversionShader = builder.build();

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    // Set up view matrices for each cubemap face
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +X
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -X
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // +Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // -Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +Z
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))  // -Z
    };

    // Create framebuffer and renderbuffer for rendering to cubemap
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_resolution, m_resolution);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // Create a simple cube mesh for rendering
    float cubeVertices[] = {
        // positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    GLuint cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Bind shader and set uniforms
    conversionShader.bind();
    glUniform1i(conversionShader.getUniformLocation("equirectangularMap"), 0);
    glUniformMatrix4fv(conversionShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));

    // Bind the equirectangular texture
    glActiveTexture(GL_TEXTURE0);
    equirectTexture.bind(GL_TEXTURE0);

    // Render to each face of the cubemap
    glViewport(0, 0, m_resolution, m_resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    for (unsigned int i = 0; i < 6; i++) {
        glUniformMatrix4fv(conversionShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_cubemap, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // Cleanup
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);

    // Restore original viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

RS_Cubemap::RS_Cubemap(RS_Cubemap&& other)
    : m_cubemap(other.m_cubemap)
    , m_resolution(other.m_resolution)
{
    other.m_cubemap = INVALID;
}

RS_Cubemap& RS_Cubemap::operator=(RS_Cubemap&& other)
{
    if (this != &other) {
        if (m_cubemap != INVALID) {
            glDeleteTextures(1, &m_cubemap);
        }

        m_cubemap = other.m_cubemap;
        m_resolution = other.m_resolution;

        other.m_cubemap = INVALID;
    }
    return *this;
}

RS_Cubemap::~RS_Cubemap()
{
    if (m_cubemap != INVALID) {
        glDeleteTextures(1, &m_cubemap);
    }
}

void RS_Cubemap::bind(GLint textureSlot) const
{
    glActiveTexture(static_cast<GLenum>(textureSlot));
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemap);
}
