#include "water_surface.h"

#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/matrix_transform.hpp>
DISABLE_WARNINGS_POP()

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace {

constexpr int MATERIAL_BINDING_POINT = 0;

RS_GPUMaterial makeWaterMaterial()
{
    RS_GPUMaterial material{};
    material.baseColor = glm::vec3(0.05f, 0.18f, 0.3f);
    material.metallic = 0.0f;
    material.roughness = 0.2f;
    material.transmission = 0.4f;
    material.emissive = glm::vec3(0.0f);
    material.textureFlags = glm::ivec4(0);
    return material;
}

} // namespace

// ----- PerlinNoise --------------------------------------------------------------------------------

PerlinNoise::PerlinNoise(uint32_t seed)
{
    std::array<int, 256> base{};
    for (int i = 0; i < 256; ++i)
        base[static_cast<size_t>(i)] = i;

    std::mt19937 rng(seed);
    std::shuffle(base.begin(), base.end(), rng);

    for (int i = 0; i < 256; ++i) {
        const size_t idx = static_cast<size_t>(i);
        m_permutation[idx] = m_permutation[256 + idx] = base[idx];
    }
}

namespace {

double fade(double t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

double lerp(double t, double a, double b)
{
    return a + t * (b - a);
}

double grad(int hash, double x, double y, double z)
{
    const int h = hash & 15;
    const double u = h < 8 ? x : y;
    const double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

} // namespace

double PerlinNoise::noise(double x, double y, double z) const
{
    const int xi = static_cast<int>(std::floor(x)) & 255;
    const int yi = static_cast<int>(std::floor(y)) & 255;
    const int zi = static_cast<int>(std::floor(z)) & 255;

    const double xf = x - std::floor(x);
    const double yf = y - std::floor(y);
    const double zf = z - std::floor(z);

    const double u = fade(xf);
    const double v = fade(yf);
    const double w = fade(zf);

    const auto perm = [this](int value) -> int {
        return m_permutation[static_cast<size_t>(value & 255)];
    };

    const int aaa = perm(perm(perm(xi) + yi) + zi);
    const int aba = perm(perm(perm(xi) + yi + 1) + zi);
    const int aab = perm(perm(perm(xi) + yi) + zi + 1);
    const int abb = perm(perm(perm(xi) + yi + 1) + zi + 1);
    const int baa = perm(perm(perm(xi + 1) + yi) + zi);
    const int bba = perm(perm(perm(xi + 1) + yi + 1) + zi);
    const int bab = perm(perm(perm(xi + 1) + yi) + zi + 1);
    const int bbb = perm(perm(perm(xi + 1) + yi + 1) + zi + 1);

    const double x1 = lerp(u, grad(aaa, xf, yf, zf), grad(baa, xf - 1, yf, zf));
    const double x2 = lerp(u, grad(aba, xf, yf - 1, zf), grad(bba, xf - 1, yf - 1, zf));
    const double y1 = lerp(v, x1, x2);

    const double x3 = lerp(u, grad(aab, xf, yf, zf - 1), grad(bab, xf - 1, yf, zf - 1));
    const double x4 = lerp(u, grad(abb, xf, yf - 1, zf - 1), grad(bbb, xf - 1, yf - 1, zf - 1));
    const double y2 = lerp(v, x3, x4);

    return lerp(w, y1, y2);
}

// ----- WaterSurface ------------------------------------------------------------------------------

WaterSurface::WaterSurface()
    : m_material(makeWaterMaterial())
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);
    glGenBuffers(1, &m_materialUbo);
}

WaterSurface::~WaterSurface()
{
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_ibo)
        glDeleteBuffers(1, &m_ibo);
    if (m_materialUbo)
        glDeleteBuffers(1, &m_materialUbo);
}

bool WaterSurface::setAmplitude(float value)
{
    value = std::max(0.005f, value);
    if (std::abs(value - m_waveAmplitude) < 1e-4f)
        return false;
    m_waveAmplitude = value;
    return true;
}

bool WaterSurface::setFrequency(float value)
{
    value = std::max(0.001f, value);
    if (std::abs(value - m_waveFrequency) < 1e-4f)
        return false;
    m_waveFrequency = value;
    return true;
}

bool WaterSurface::setSpeed(float value)
{
    if (std::abs(value - m_waveSpeed) < 1e-4f)
        return false;
    m_waveSpeed = value;
    return true;
}

bool WaterSurface::setTileSize(float value)
{
    value = std::clamp(value, 20.0f, 400.0f);
    if (std::abs(value - m_extent) < 1e-4f)
        return false;
    m_extent = value;
    m_needsRebuild = true;
    return true;
}

bool WaterSurface::setHeightOffset(float value)
{
    if (std::abs(value - m_heightOffset) < 1e-4f)
        return false;
    m_heightOffset = value;
    return true;
}

void WaterSurface::rebuild()
{
    const size_t vertexCount = static_cast<size_t>(m_resolution + 1) * static_cast<size_t>(m_resolution + 1);
    m_vertices.resize(vertexCount);
    m_indices.clear();
    m_indices.reserve(static_cast<size_t>(m_resolution) * static_cast<size_t>(m_resolution) * 6);

    const float step = m_extent / static_cast<float>(m_resolution);

    for (int z = 0; z <= m_resolution; ++z) {
        for (int x = 0; x <= m_resolution; ++x) {
            const size_t idx = static_cast<size_t>(z) * static_cast<size_t>(m_resolution + 1) + static_cast<size_t>(x);

            float localX = -0.5f * m_extent + step * static_cast<float>(x);
            float localZ = -0.5f * m_extent + step * static_cast<float>(z);

            float worldX = m_center.x + localX;
            float worldZ = m_center.y + localZ;
            float height = sampleHeight(worldX, worldZ);

            m_vertices[idx].position = glm::vec3(localX, height + m_heightOffset, localZ);
            m_vertices[idx].texCoord = glm::vec2(static_cast<float>(x) / m_resolution, static_cast<float>(z) / m_resolution);
            m_vertices[idx].normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    for (int z = 0; z < m_resolution; ++z) {
        for (int x = 0; x < m_resolution; ++x) {
            uint32_t topLeft = static_cast<uint32_t>(z * (m_resolution + 1) + x);
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = topLeft + static_cast<uint32_t>(m_resolution + 1);
            uint32_t bottomRight = bottomLeft + 1;

            m_indices.push_back(topLeft);
            m_indices.push_back(bottomLeft);
            m_indices.push_back(topRight);

            m_indices.push_back(topRight);
            m_indices.push_back(bottomLeft);
            m_indices.push_back(bottomRight);
        }
    }

    m_indexCount = static_cast<GLsizei>(m_indices.size());

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(uint32_t)), m_indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoord)));

    glBindVertexArray(0);

    m_needsRebuild = false;
}

void WaterSurface::updateBuffers()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data());
}

float WaterSurface::sampleHeight(float worldX, float worldZ) const
{
    const double nx = static_cast<double>(worldX) * static_cast<double>(m_waveFrequency);
    const double ny = static_cast<double>(m_time) * static_cast<double>(m_waveSpeed);
    const double nz = static_cast<double>(worldZ) * static_cast<double>(m_waveFrequency);
    const double height = m_noise.noise(nx, ny, nz);
    return static_cast<float>(height) * m_waveAmplitude;
}

glm::vec3 WaterSurface::computeNormal(int x, int z, float step, const std::vector<float>& heights) const
{
    const auto idx = [this](int xi, int zi) {
        xi = std::clamp(xi, 0, m_resolution);
        zi = std::clamp(zi, 0, m_resolution);
        return static_cast<size_t>(zi) * static_cast<size_t>(m_resolution + 1) + static_cast<size_t>(xi);
    };

    float hL = heights[idx(x - 1, z)];
    float hR = heights[idx(x + 1, z)];
    float hD = heights[idx(x, z - 1)];
    float hU = heights[idx(x, z + 1)];

    glm::vec3 normal(-(hR - hL), 2.0f * step, -(hU - hD));
    return glm::normalize(normal);
}

void WaterSurface::uploadMaterial()
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_materialUbo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(RS_GPUMaterial), &m_material, GL_DYNAMIC_DRAW);
}

void WaterSurface::update(const glm::vec3& focusPosition, float deltaTime)
{
    if (!m_enabled)
        return;

    m_time += deltaTime;
    glm::vec2 newCenter(focusPosition.x, focusPosition.z);
    if (glm::distance(newCenter, m_center) > 5.0f) {
        m_center = newCenter;
        m_needsRebuild = true;
    }

    if (m_needsRebuild) {
        rebuild();
    }

    const float step = m_extent / static_cast<float>(m_resolution);
    const size_t vertexCount = static_cast<size_t>(m_resolution + 1) * static_cast<size_t>(m_resolution + 1);
    std::vector<float> heights(vertexCount);

    for (int z = 0; z <= m_resolution; ++z) {
        for (int x = 0; x <= m_resolution; ++x) {
            const size_t idx = static_cast<size_t>(z) * static_cast<size_t>(m_resolution + 1) + static_cast<size_t>(x);

            float localX = -0.5f * m_extent + step * static_cast<float>(x);
            float localZ = -0.5f * m_extent + step * static_cast<float>(z);
            float worldX = m_center.x + localX;
            float worldZ = m_center.y + localZ;

            float height = sampleHeight(worldX, worldZ);
            heights[idx] = height;

            m_vertices[idx].position = glm::vec3(localX, height + m_heightOffset, localZ);
            m_vertices[idx].texCoord = glm::vec2(static_cast<float>(x) / m_resolution, static_cast<float>(z) / m_resolution);
        }
    }

    for (int z = 0; z <= m_resolution; ++z) {
        for (int x = 0; x <= m_resolution; ++x) {
            const size_t idx = static_cast<size_t>(z) * static_cast<size_t>(m_resolution + 1) + static_cast<size_t>(x);
            m_vertices[idx].normal = computeNormal(x, z, step, heights);
        }
    }

    updateBuffers();
}

void WaterSurface::draw(const Shader& shader, const glm::mat4& viewProjectionMatrix)
{
    if (!m_enabled)
        return;

    uploadMaterial();
    shader.bindUniformBlock("Material", MATERIAL_BINDING_POINT, m_materialUbo);

    const glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(m_center.x, 0.0f, m_center.y));
    const glm::mat4 mvpMatrix = viewProjectionMatrix * modelMatrix;
    const glm::mat3 normalMatrix = glm::mat3(1.0f);

    const GLint mvpLoc = shader.getUniformLocation("mvpMatrix");
    const GLint modelLoc = shader.getUniformLocation("modelMatrix");
    const GLint normalLoc = shader.getUniformLocation("normalModelMatrix");
    const GLint hasTexCoordsLoc = shader.getUniformLocation("hasTexCoords");
    const GLint useMaterialLoc = shader.getUniformLocation("useMaterial");

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &mvpMatrix[0][0]);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &modelMatrix[0][0]);
    glUniformMatrix3fv(normalLoc, 1, GL_FALSE, &normalMatrix[0][0]);
    glUniform1i(hasTexCoordsLoc, 1);
    glUniform1i(useMaterialLoc, 1);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void WaterSurface::drawEnvironment(const Shader& shader, const glm::mat4& viewProjectionMatrix)
{
    draw(shader, viewProjectionMatrix);
}

void WaterSurface::drawDepth(const Shader& shader, const glm::mat4& viewProjectionMatrix)
{
    if (!m_enabled)
        return;

    const glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(m_center.x, 0.0f, m_center.y));
    const glm::mat4 mvpMatrix = viewProjectionMatrix * modelMatrix;
    const GLint mvpLoc = shader.getUniformLocation("mvpMatrix");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &mvpMatrix[0][0]);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void WaterSurface::drawDepthCubemap(const Shader& shader)
{
    if (!m_enabled)
        return;

    const glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(m_center.x, 0.0f, m_center.y));
    const GLint modelLoc = shader.getUniformLocation("modelMatrix");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &modelMatrix[0][0]);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
