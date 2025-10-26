//
// Created by Rafayel on 10/11/2025.
//

#include "model.h"
#include <framework/mesh.h>
#include <iostream>
#include <functional>
#include <algorithm>
#include <chrono>
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
DISABLE_WARNINGS_POP()

// Forward declarations
std::vector<std::vector<int>> buildPascalTriangle(int n);

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

void RS_Model::setPosition(const glm::vec3& pos)
{
    // Extract current rotation and scale, then rebuild with new position
    m_model_matrix[3] = glm::vec4(pos, 1.0f);
}

void RS_Model::setRotation(const glm::vec3& eulerAngles)
{
    // Build rotation matrix from Euler angles (in radians)
    glm::mat4 rotation = glm::mat4(1.0f);
    rotation = glm::rotate(rotation, eulerAngles.z, glm::vec3(0.0f, 0.0f, 1.0f));
    rotation = glm::rotate(rotation, eulerAngles.y, glm::vec3(0.0f, 1.0f, 0.0f));
    rotation = glm::rotate(rotation, eulerAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));

    // Preserve position
    glm::vec3 position = glm::vec3(m_model_matrix[3]);
    m_model_matrix = rotation;
    m_model_matrix[3] = glm::vec4(position, 1.0f);
}

void RS_Model::setScale(const glm::vec3& scale)
{
    // Apply scale to the existing matrix
    m_model_matrix = glm::scale(m_model_matrix, scale);
}

void RS_Model::setAnimationCurve(const std::vector<glm::vec3>& curvePoints)
{
    m_animateCurvePoints = curvePoints;
    // Rebuild Pascal triangle when curve changes
    if (!curvePoints.empty()) {
        m_pascalTriangle = buildPascalTriangle(curvePoints.size());
    }
}

void RS_Model::enableAnimation(bool enable)
{
    if (enable && !m_animationEnabled) {
        // Starting animation - record start time
        m_animationStartTime = std::chrono::steady_clock::now();
        m_animationStarted = true;
    } else if (!enable) {
        // Stopping animation - reset flag
        m_animationStarted = false;
    }
    m_animationEnabled = enable;
}

void RS_Model::setMeshHierarchy(const std::vector<MeshNode>& hierarchy)
{
    m_hierarchy = hierarchy;
    m_worldTransforms.resize(m_meshes.size(), glm::mat4(1.0f));
    m_useHierarchy = !hierarchy.empty();
    if (m_useHierarchy) {
        computeWorldTransforms();
    }
}

void RS_Model::setMeshParent(int meshIndex, int parentMeshIndex, const glm::mat4& localTransform)
{
    if (meshIndex < 0 || meshIndex >= static_cast<int>(m_meshes.size())) {
        return; // Invalid mesh index
    }

    // Initialize hierarchy if not already done
    if (m_hierarchy.empty()) {
        m_hierarchy.resize(m_meshes.size());
        for (size_t i = 0; i < m_meshes.size(); i++) {
            m_hierarchy[i].meshIndex = static_cast<int>(i);
            m_hierarchy[i].parentIndex = -1;
            m_hierarchy[i].localTransform = glm::mat4(1.0f);
        }
        m_worldTransforms.resize(m_meshes.size(), glm::mat4(1.0f));
    }

    // Set parent-child relationship
    m_hierarchy[meshIndex].parentIndex = parentMeshIndex;
    m_hierarchy[meshIndex].localTransform = localTransform;

    // Update parent's child list
    if (parentMeshIndex >= 0 && parentMeshIndex < static_cast<int>(m_hierarchy.size())) {
        auto& children = m_hierarchy[parentMeshIndex].childIndices;
        if (std::find(children.begin(), children.end(), meshIndex) == children.end()) {
            children.push_back(meshIndex);
        }
    }

    m_useHierarchy = true;
    computeWorldTransforms();
}

void RS_Model::computeWorldTransforms()
{
    if (m_hierarchy.empty() || m_worldTransforms.size() != m_meshes.size()) {
        return;
    }

    // Lambda for recursive world transform computation
    std::function<void(int, const glm::mat4&)> computeNode = [&](int nodeIndex, const glm::mat4& parentWorld) {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_hierarchy.size())) {
            return;
        }

        const MeshNode& node = m_hierarchy[nodeIndex];

        // Compute world transform: parent * local
        glm::mat4 worldTransform = parentWorld * node.localTransform;

        // Store for the mesh
        if (node.meshIndex >= 0 && node.meshIndex < static_cast<int>(m_worldTransforms.size())) {
            m_worldTransforms[node.meshIndex] = worldTransform;
        }

        // Recursively compute children
        for (int childIndex : node.childIndices) {
            computeNode(childIndex, worldTransform);
        }
    };

    // Compute world transforms starting from root nodes (those with no parent)
    for (size_t i = 0; i < m_hierarchy.size(); i++) {
        if (m_hierarchy[i].parentIndex == -1) {
            computeNode(static_cast<int>(i), glm::mat4(1.0f));
        }
    }
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
glm::mat4 animate(glm::mat4 matrix, std::vector<glm::vec3> curve, float t, const std::vector<std::vector<int>>& triangle) {
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
    glm::vec3 direction = glm::normalize(tangent);

    // Build orthonormal basis from tangent direction
    // Handle edge case where direction is parallel to world up
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(direction, worldUp)) > 0.999f) {
        worldUp = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 right = glm::normalize(glm::cross(worldUp, direction));
	glm::vec3 up = glm::cross(direction, right);

    // Build transformation matrix: rotation to orient along curve, then translation to position
    glm::mat4 orientation(1.0f);
    orientation[0] = glm::vec4(right, 0.0f);      // X axis
    orientation[1] = glm::vec4(up, 0.0f);         // Y axis
    orientation[2] = glm::vec4(direction, 0.0f);  // Z axis (forward)
    orientation[3] = glm::vec4(position, 1.0f);   // Position

    // Apply base model matrix first, then animation transform
    return orientation * matrix;
}

void RS_Model::draw(const Shader& drawShader, const glm::mat4& viewProjectionMatrix)
{
    // Compute MVP matrix
    glm::mat4 model_matrix = m_model_matrix;

    if (m_animationEnabled && m_animationStarted && !m_animateCurvePoints.empty() && !m_pascalTriangle.empty()) {
        // Calculate elapsed time since animation started
        auto now = std::chrono::steady_clock::now();
        float elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(now - m_animationStartTime).count();

        // Normalize to [0, 1] range, looping every m_animateTime seconds
        float t = fmod(elapsedTime / m_animateTime, 1.0f);

        // Use cached Pascal triangle
        model_matrix = animate(model_matrix, m_animateCurvePoints, t, m_pascalTriangle);
    }

    const glm::mat4 mvpMatrix = viewProjectionMatrix * model_matrix;

    // Compute normal transformation matrix (inverse transpose of model matrix)
    const glm::mat3 normalModelMatrix = glm::transpose(glm::inverse(glm::mat3(model_matrix)));

    // Set per-model uniforms
    glUniformMatrix4fv(drawShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
    glUniformMatrix4fv(drawShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(model_matrix));
    glUniformMatrix3fv(drawShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE,
                       glm::value_ptr(normalModelMatrix));

    for (size_t i = 0; i < m_meshes.size(); i++)
    {
        // Compute per-mesh transform (use hierarchy if enabled)
        glm::mat4 meshTransform = model_matrix;
        if (m_useHierarchy && i < m_worldTransforms.size()) {
            meshTransform = model_matrix * m_worldTransforms[i];
        }

        const glm::mat4 meshMvpMatrix = viewProjectionMatrix * meshTransform;
        const glm::mat3 meshNormalMatrix = glm::transpose(glm::inverse(glm::mat3(meshTransform)));

        // Update uniforms for this mesh
        glUniformMatrix4fv(drawShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(meshMvpMatrix));
        glUniformMatrix4fv(drawShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(meshTransform));
        glUniformMatrix3fv(drawShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(meshNormalMatrix));

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

void RS_Model::addMesh(GPUMesh&& mesh)
{
    m_meshes.push_back(std::move(mesh));
}

void RS_Model::addMaterial(RS_Material&& material)
{
    m_materials.push_back(std::move(material));
}
