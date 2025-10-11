//
// Created by Rafayel on 10/11/2025.
//

#ifndef COMPUTERGRAPHICS_RSMODEL_H
#define COMPUTERGRAPHICS_RSMODEL_H
#include "mesh.h"
#include "texture.h"

// GPU-compatible material struct (std140 layout)
// Note: Textures are bound separately, not stored in the uniform buffer!
struct RS_GPUMaterial
{
    alignas(16) glm::vec3 baseColor;    // offset 0
    float metallic;                      // offset 12
    float roughness;                     // offset 16
    float transmission;                  // offset 20
    alignas(16) glm::vec3 emissive;     // offset 32 (needs padding to align to vec3)
    float _padding;                      // offset 44

    // Texture flags (use these to know which textures are bound in shader)
    alignas(16) glm::ivec4 textureFlags; // offset 48: [hasBaseColor, hasNormal, hasMetallicRoughness, hasEmissive]
};

// CPU-side material that holds both GPU data and texture references
struct RS_Material
{
    RS_GPUMaterial gpuData;

    // CPU-side texture references (bind these separately when rendering)
    Texture* baseColorTex = nullptr;
    Texture* normalTex = nullptr;
    Texture* metallicRoughnessTex = nullptr;
    Texture* emissiveTex = nullptr;
};

class RS_Model
{
public:

    RS_Model();

    void draw(const Shader& drawShader, const glm::mat4& viewProjectionMatrix);
    void addMesh(GPUMesh&& mesh);
    void addMaterial(const RS_Material& material);

    // Getters for ImGui editing
    std::vector<RS_Material>& getMaterials() { return m_materials; }
    const std::vector<RS_Material>& getMaterials() const { return m_materials; }
    size_t getMeshCount() const { return m_meshes.size(); }

private:
    std::vector<GPUMesh> m_meshes;
    std::vector<RS_Material> m_materials;
    glm::mat4 m_model_matrix { glm::mat4(1.0f) };
    GLuint m_material_UBO { 0 };
};


#endif //COMPUTERGRAPHICS_RSMODEL_H
