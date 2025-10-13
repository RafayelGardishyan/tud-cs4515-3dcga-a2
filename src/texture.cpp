#include "texture.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <stb/stb_image.h>
DISABLE_WARNINGS_POP()
#include <iostream>
#include <string>
#include <algorithm>

RS_Texture::RS_Texture(std::filesystem::path filePath, bool isHDR)
    : m_isHDR(isHDR)
{
    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "Texture file \"" << filePath << "\" does not exist!" << std::endl;
        throw ImageLoadingException("Texture file does not exist");
    }

    const std::string filePathStr = filePath.string();

    // Auto-detect HDR format
    if (!isHDR) {
        std::string ext = filePath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        m_isHDR = (ext == ".hdr" || ext == ".exr");
    }

    // Load image data
    void* data = nullptr;

    if (m_isHDR) {
        // Load as floating-point HDR data
        float* hdrData = stbi_loadf(filePathStr.c_str(), &m_width, &m_height, &m_channels, 0);

        if (!hdrData) {
            std::cerr << "Failed to read HDR texture \"" << filePath << "\" using stb_image.h" << std::endl;
            std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
            throw ImageLoadingException("Failed to load HDR texture");
        }

        data = hdrData;
    } else {
        // Load
        unsigned char* ldrData = stbi_load(filePathStr.c_str(), &m_width, &m_height, &m_channels, 0);

        if (!ldrData) {
            std::cerr << "Failed to read LDR texture \"" << filePath << "\" using stb_image.h" << std::endl;
            std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
            throw ImageLoadingException("Failed to load LDR texture");
        }

        data = ldrData;
    }

    // Create OpenGL texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload texture data based on format and channels
    if (m_isHDR) {
        switch (m_channels) {
            case 1:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, data);
                break;
            case 3:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_width, m_height, 0, GL_RGB, GL_FLOAT, data);
                break;
            case 4:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, data);
                break;
            default:
                stbi_image_free(data);
                std::cerr << "Unsupported number of channels: " << m_channels << std::endl;
                throw ImageLoadingException("Unsupported number of channels");
        }
    } else {
        switch (m_channels) {
            case 1:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_width, m_height, 0, GL_RED, GL_UNSIGNED_BYTE, data);
                break;
            case 3:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
                break;
            case 4:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                break;
            default:
                stbi_image_free(data);
                std::cerr << "Unsupported number of channels: " << m_channels << std::endl;
                throw ImageLoadingException("Unsupported number of channels");
        }
    }

    // Generate mipmaps
    glGenerateMipmap(GL_TEXTURE_2D);

    // Free image data
    stbi_image_free(data);

    std::cout << "Loaded " << (m_isHDR ? "HDR" : "LDR") << " texture: " << filePath
              << " (" << m_width << "x" << m_height << ", " << m_channels << " channels)" << std::endl;
}

RS_Texture::RS_Texture(RS_Texture&& other)
    : m_texture(other.m_texture)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_channels(other.m_channels)
    , m_isHDR(other.m_isHDR)
{
    other.m_texture = INVALID;
}

RS_Texture& RS_Texture::operator=(RS_Texture&& other)
{
    if (this != &other) {
        // Delete existing texture
        if (m_texture != INVALID) {
            glDeleteTextures(1, &m_texture);
        }

        // Move data
        m_texture = other.m_texture;
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_isHDR = other.m_isHDR;

        // Invalidate other
        other.m_texture = INVALID;
    }
    return *this;
}

RS_Texture::~RS_Texture()
{
    if (m_texture != INVALID) {
        glDeleteTextures(1, &m_texture);
    }
}

void RS_Texture::bind(GLint textureSlot) const
{
    glActiveTexture(textureSlot);
    glBindTexture(GL_TEXTURE_2D, m_texture);
}

void RS_Texture::setEnvironmentMapWrapping()
{
    glBindTexture(GL_TEXTURE_2D, m_texture);
    // For equirectangular maps: repeat horizontally, clamp vertically
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Use trilinear filtering with mipmaps for proper LOD sampling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Regenerate mipmaps after changing parameters
    glGenerateMipmap(GL_TEXTURE_2D);
}
