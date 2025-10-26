//
// Created by Rafayel on 10/11/2025.
//

#ifndef COMPUTERGRAPHICS_RSMODEL_H
#define COMPUTERGRAPHICS_RSMODEL_H
#include "mesh.h"
#include "texture.h"
#include <memory>
#include <vector>
#include <chrono>

inline glm::ivec4 RS_HAS_COLOR_TEX = {1, 0, 0, 0};
inline glm::ivec4 RS_HAS_NORMAL_TEX = {0, 1, 0, 0};
inline glm::ivec4 RS_HAS_METALLIC_ROUGHNESS_TEX = {0, 0, 1, 0};
inline glm::ivec4 RS_HAS_EMISSIVE_TEX = {0, 0, 0, 1};

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

    std::unique_ptr<RS_Texture> baseColorTex = nullptr;
    std::unique_ptr<RS_Texture> normalTex = nullptr;
    std::unique_ptr<RS_Texture> metallicTex = nullptr;
    std::unique_ptr<RS_Texture> roughnessTex = nullptr;

    // Helper function to create material from framework mesh material
    static RS_Material createFromMesh(const GPUMesh& mesh);
};

// Hierarchical mesh node for parent-child transformations
struct MeshNode
{
    int meshIndex = -1;                      // Index into RS_Model::m_meshes (-1 for no mesh)
    glm::mat4 localTransform = glm::mat4(1.0f); // Transform relative to parent
    int parentIndex = -1;                    // Index of parent node (-1 for root)
    std::vector<int> childIndices;           // Indices of child nodes
};

class RS_Model
{
public:

    RS_Model();

    void draw(const Shader& drawShader, const glm::mat4& viewProjectionMatrix);
    void addMesh(GPUMesh&& mesh);
    void addMaterial(RS_Material&& material);
    void setAnimationCurve(const std::vector<glm::vec3>& curvePoints);
    float m_animateTime{ 5.0f };

    // Model transformation API
    void setModelMatrix(const glm::mat4& matrix) { m_model_matrix = matrix; }
    glm::mat4 getModelMatrix() const { return m_model_matrix; }
    void setPosition(const glm::vec3& pos);
    void setRotation(const glm::vec3& eulerAngles);
    void setScale(const glm::vec3& scale);

    // Hierarchy management
    void setMeshHierarchy(const std::vector<MeshNode>& hierarchy);
    void setMeshParent(int meshIndex, int parentMeshIndex, const glm::mat4& localTransform = glm::mat4(1.0f));
    const std::vector<glm::mat4>& getWorldTransforms() const { return m_worldTransforms; }

    // Getters for ImGui editing
    std::vector<RS_Material>& getMaterials() { return m_materials; }
    const std::vector<RS_Material>& getMaterials() const { return m_materials; }
    size_t getMeshCount() const { return m_meshes.size(); }
	bool getAnimationEnabled() const { return m_animationEnabled; }
	void enableAnimation(bool enable);

private:
    void computeWorldTransforms(); // Compute world transforms from hierarchy

    std::vector<GPUMesh> m_meshes;
    std::vector<RS_Material> m_materials;
    glm::mat4 m_model_matrix { glm::mat4(1.0f) };
    GLuint m_material_UBO { 0 };
	bool m_animationEnabled{ false };
	std::vector<glm::vec3> m_animateCurvePoints;
	std::vector<std::vector<int>> m_pascalTriangle; // Cached Pascal triangle for Bezier calculations
	std::chrono::steady_clock::time_point m_animationStartTime; // When animation was enabled
	bool m_animationStarted{ false };            // Whether animation timing has been initialized

    // Hierarchical transformation system
    std::vector<MeshNode> m_hierarchy;           // Mesh hierarchy nodes
    std::vector<glm::mat4> m_worldTransforms;    // Cached world transforms for each mesh
    bool m_useHierarchy{ false };                // Whether to use hierarchical transforms
};


#endif //COMPUTERGRAPHICS_RSMODEL_H
