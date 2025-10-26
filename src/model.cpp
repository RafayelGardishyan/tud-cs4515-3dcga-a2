//
// Created by Rafayel on 10/11/2025.
//

#include "model.h"
#include <framework/mesh.h>
#include <iostream>
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
DISABLE_WARNINGS_POP()
#include <chrono>
#include <cmath>

RS_Material RS_Material::createFromMesh(const GPUMesh& mesh)
{
    RS_Material material;
    const Material& cpuMaterial = mesh.getMaterial();

    material.gpuData.baseColor = cpuMaterial.kd;
    material.gpuData.metallic = 0.0f;
    material.gpuData.roughness = 0.5f;
    material.gpuData.transmission = 0.0f;
    material.gpuData.emissive = glm::vec3(0.0f);
    material.gpuData.textureFlags = glm::ivec4(0);

    if (cpuMaterial.kdTexture) {
        try {
            material.baseColorTex = std::make_unique<RS_Texture>(*cpuMaterial.kdTexture);
            material.gpuData.textureFlags += RS_HAS_COLOR_TEX;
            std::cout << "Loaded base color texture from mesh material" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load base color texture: " << e.what() << std::endl;
        }
    }

    if (cpuMaterial.norTexture)
    {
        try
        {
            material.normalTex = std::make_unique<RS_Texture>(*cpuMaterial.norTexture);
            material.gpuData.textureFlags += RS_HAS_NORMAL_TEX;
            std::cout << "Loaded normal texture from mesh material" << std::endl;
        } catch (const std::exception& e)
        {
            std::cerr << "Failed to load normal color texture: " << e.what() << std::endl;
        }
    }

    // Load metallic texture
    if (cpuMaterial.metTexture)
    {
        try
        {
            material.metallicTex = std::make_unique<RS_Texture>(*cpuMaterial.metTexture);
            material.gpuData.textureFlags += RS_HAS_METALLIC_ROUGHNESS_TEX;
            std::cout << "Loaded metallic texture from mesh material" << std::endl;
        } catch (const std::exception& e)
        {
            std::cerr << "Failed to load metallic texture: " << e.what() << std::endl;
        }
    }

    // Load roughness texture
    if (cpuMaterial.roughTexture)
    {
        try
        {
            material.roughnessTex = std::make_unique<RS_Texture>(*cpuMaterial.roughTexture);
            std::cout << "Loaded roughness texture from mesh material" << std::endl;
        } catch (const std::exception& e)
        {
            std::cerr << "Failed to load roughness texture: " << e.what() << std::endl;
        }
    }

    std::cout << material.gpuData.textureFlags.y << std::endl;

    return material;
}

RS_Model::RS_Model()
{
    glGenBuffers(1, &m_material_UBO);
}

std::vector<std::vector<int>> buildPascalTriangle(int n) {
    std::vector<std::vector<int>> triangle = {};

    for(int i = 0; i <= n; i++) {
        std::vector<int> row = {};
        for (int j = 0; j <= n; j++) {
            if (j > i) {
                row.push_back(0);
                continue;
            }
            if (j == 0 || j == i) {
                row.push_back(1);
                continue;
            }
            row.push_back(triangle[i - 1][j - 1] + triangle[i - 1][j]);
        }
        triangle.push_back(row);
	}
    
    return triangle;
}

// Animate a model along a given curve
glm::mat4 animate(glm::mat4 matrix, std::vector<glm::vec3> curve, float t, const std::vector<std::vector<int>> triangle) {
	glm::vec3 position(0.0f);
    glm::vec3 tangent(0.0f);

	// Calculate position using Bezier curve formula
    for(int i = 0; i < curve.size(); i++) {
		position += float(float(triangle[curve.size() - 1][i]) * pow(1.0 - t, curve.size() - 1 - i) * pow(t, i)) * curve[i];
	}
	// Calculate tangent using derivative of Bezier curve formula
    for (int i = 0; i < curve.size() - 1; i++) {
        tangent += float(float(triangle[curve.size() - 2][i]) * pow(1.0f - t, curve.size() - 2 - i) * pow(t, i)) * (curve[i + 1] - curve[i]);
    }
	tangent *= float(curve.size() - 1);
    glm::vec3 direction = normalize(tangent);

    glm::vec3 right = normalize(cross(glm::vec3(0.0f, 1.0f, 0.0f), direction));
	glm::vec3 up = cross(direction, right);

    glm::mat4 rot(1.0f);
    rot[0] = glm::vec4(right, 0.0f);    // right direction
    rot[1] = glm::vec4(up, 0.0f);       // up direction
    rot[2] = glm::vec4(direction, 0.0f);// forward direction

    matrix = glm::translate(matrix, position);
    return matrix * rot;
}

void RS_Model::draw(const Shader& drawShader, const glm::mat4& viewProjectionMatrix)
{
    const glm::mat4 baseModelMatrix = evaluateModelMatrix();

    for (size_t i = 0; i < m_meshes.size(); i++)
    {
        const glm::mat4 meshModelMatrix = evaluateMeshSpecificMatrix(i, baseModelMatrix);
        const glm::mat4 meshMvpMatrix = viewProjectionMatrix * meshModelMatrix;
        const glm::mat3 meshNormalMatrix = glm::transpose(glm::inverse(glm::mat3(meshModelMatrix)));

        glUniformMatrix4fv(drawShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(meshMvpMatrix));
        glUniformMatrix4fv(drawShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(meshModelMatrix));
        glUniformMatrix3fv(drawShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE,
            glm::value_ptr(meshNormalMatrix));

        // Bind material i
        const RS_Material& material = m_materials[i];

        // Upload material uniform buffer
        glBindBuffer(GL_UNIFORM_BUFFER, m_material_UBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(RS_GPUMaterial), &material.gpuData, GL_DYNAMIC_DRAW);
        drawShader.bindUniformBlock("Material", 0, m_material_UBO);

        // Bind textures
        if (material.baseColorTex)
        {
            material.baseColorTex->bind(GL_TEXTURE1);
            GLint texLoc = drawShader.getUniformLocation("colorMap");
            if (texLoc != -1)
            {
                glUniform1i(texLoc, 1);
            }
        }

        if (material.normalTex)
        {
            material.normalTex->bind(GL_TEXTURE2);
            GLint texLoc = drawShader.getUniformLocation("normalMap");
            if (texLoc != -1)
            {
                glUniform1i(texLoc, 2);
            }
        }

        if (material.metallicTex)
        {
            material.metallicTex->bind(GL_TEXTURE3);
            GLint texLoc = drawShader.getUniformLocation("metallicMap");
            if (texLoc != -1)
            {
                glUniform1i(texLoc, 3);
            }
        }

        if (material.roughnessTex)
        {
            material.roughnessTex->bind(GL_TEXTURE4);
            GLint texLoc = drawShader.getUniformLocation("roughnessMap");
            if (texLoc != -1)
            {
                glUniform1i(texLoc, 4);
            }
        }


        glUniform1i(drawShader.getUniformLocation("hasTexCoords"), m_meshes[i].hasTextureCoords() ? 1 : 0);
        glUniform1i(drawShader.getUniformLocation("useMaterial"), 1);

        // Draw mesh
        m_meshes[i].draw(drawShader);
    }
}

void RS_Model::drawDepth(const Shader& depthShader, const glm::mat4& viewProjectionMatrix)
{
    const glm::mat4 baseModelMatrix = evaluateModelMatrix();
    const GLint mvpLocation = depthShader.getUniformLocation("mvpMatrix");

    for (size_t i = 0; i < m_meshes.size(); i++) {
        const glm::mat4 meshModelMatrix = evaluateMeshSpecificMatrix(i, baseModelMatrix);
        const glm::mat4 meshMvpMatrix = viewProjectionMatrix * meshModelMatrix;
        glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(meshMvpMatrix));
        m_meshes[i].draw(depthShader);
    }
}

void RS_Model::drawDepthCubemap(const Shader& depthCubemapShader)
{
    const glm::mat4 baseModelMatrix = evaluateModelMatrix();
    const GLint modelLocation = depthCubemapShader.getUniformLocation("modelMatrix");

    for (size_t i = 0; i < m_meshes.size(); i++) {
        const glm::mat4 meshModelMatrix = evaluateMeshSpecificMatrix(i, baseModelMatrix);
        glUniformMatrix4fv(
            modelLocation,
            1,
            GL_FALSE,
            glm::value_ptr(meshModelMatrix));
        m_meshes[i].draw(depthCubemapShader);
    }
}

void RS_Model::addMesh(GPUMesh&& mesh)
{
    m_meshes.push_back(std::move(mesh));
}

void RS_Model::addMaterial(RS_Material&& material)
{
    m_materials.push_back(std::move(material));
}

glm::mat4 RS_Model::evaluateModelMatrix() const
{
    glm::mat4 modelMatrix = m_model_matrix;

    if (m_animationEnabled && !m_animateCurvePoints.empty()) {
        float t = std::chrono::duration<float>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        t = fmod(t / m_animateTime, 1.0f);
        const std::vector<std::vector<int>> pascalTriangle = buildPascalTriangle(m_animateCurvePoints.size());
        modelMatrix = animate(modelMatrix, m_animateCurvePoints, t, pascalTriangle);
    }

    return modelMatrix;
}

glm::mat4 RS_Model::evaluateMeshSpecificMatrix(size_t meshIndex, const glm::mat4& baseMatrix) const
{
    if (meshIndex == 2 && m_animationEnabled) {
        float t = std::chrono::duration<float>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        glm::mat4 altModelMatrix = glm::translate(
            glm::rotate(
                glm::translate(
                    glm::mat4(1.0f), { 0.0f, -1.0f, 0.0f }
                ), glm::sin(t) * 0.1f, { 1, 0, 0 }),
            { 0.0f, 1.0f, 0.0f }
        );

        return altModelMatrix * baseMatrix;
    }

    return baseMatrix;
}
