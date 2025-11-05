#pragma once

#include "mesh.h"
#include "model.h"

#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
#include <glm/glm.hpp>
DISABLE_WARNINGS_POP()

#include <array>
#include <cstdint>
#include <vector>

#include <framework/shader.h>

class PerlinNoise
{
public:
    explicit PerlinNoise(uint32_t seed = 0);

    double noise(double x, double y, double z) const;

private:
    std::array<int, 512> m_permutation{};
};

class WaterSurface
{
public:
    WaterSurface();
    ~WaterSurface();

    WaterSurface(const WaterSurface&) = delete;
    WaterSurface& operator=(const WaterSurface&) = delete;

    void update(const glm::vec3& focusPosition, float deltaTime);
    void draw(const Shader& shader, const glm::mat4& viewProjectionMatrix);
    void drawEnvironment(const Shader& shader, const glm::mat4& viewProjectionMatrix);
    void drawDepth(const Shader& shader, const glm::mat4& viewProjectionMatrix);
    void drawDepthCubemap(const Shader& shader);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    bool setAmplitude(float value);
    bool setFrequency(float value);
    bool setSpeed(float value);
    bool setTileSize(float value);
    bool setHeightOffset(float value);

    float getAmplitude() const { return m_waveAmplitude; }
    float getFrequency() const { return m_waveFrequency; }
    float getSpeed() const { return m_waveSpeed; }
    float getTileSize() const { return m_extent; }
    float getHeightOffset() const { return m_heightOffset; }

private:
    void rebuild();
    void updateBuffers();
    float sampleHeight(float worldX, float worldZ) const;
    glm::vec3 computeNormal(int x, int z, float step, const std::vector<float>& heights) const;
    void uploadMaterial();

private:
    bool m_enabled{ true };
    bool m_needsRebuild{ true };

    glm::vec2 m_center{ 0.0f };
    float m_extent{ 120.0f };
    int m_resolution{ 96 };

    float m_waveAmplitude{ 0.1f };
    float m_waveFrequency{ 0.35f };
    float m_waveSpeed{ 0.6f };
    float m_heightOffset{ -1.5f };
    float m_time{ 0.0f };

    GLuint m_vao{ 0 };
    GLuint m_vbo{ 0 };
    GLuint m_ibo{ 0 };
    GLsizei m_indexCount{ 0 };

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    GLuint m_materialUbo{ 0 };
    RS_GPUMaterial m_material{};

    PerlinNoise m_noise;
};
