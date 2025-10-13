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
DISABLE_WARNINGS_POP()

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

    return material;
}

RS_Model::RS_Model()
{
    glGenBuffers(1, &m_material_UBO);
}


void RS_Model::draw(const Shader& drawShader, const glm::mat4& viewProjectionMatrix)
{
    // Compute MVP matrix
    const glm::mat4 mvpMatrix = viewProjectionMatrix * m_model_matrix;

    // Compute normal transformation matrix (inverse transpose of model matrix)
    const glm::mat3 normalModelMatrix = glm::transpose(glm::inverse(glm::mat3(m_model_matrix)));

    // Set per-model uniforms
    glUniformMatrix4fv(drawShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
    glUniformMatrix4fv(drawShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_model_matrix));
    glUniformMatrix3fv(drawShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE,
                       glm::value_ptr(normalModelMatrix));

    for (size_t i = 0; i < m_meshes.size(); i++)
    {
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
