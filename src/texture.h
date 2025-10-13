#pragma once
#include <framework/disable_all_warnings.h>

#include "framework/image.h"
DISABLE_WARNINGS_PUSH()
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()
#include <exception>
#include <filesystem>
#include <framework/opengl_includes.h>


struct ImageLoadingException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class RS_Texture {
public:
    RS_Texture(std::filesystem::path filePath, bool isHDR = false);
    RS_Texture(const Image& image); // Create texture from framework
    RS_Texture(const RS_Texture&) = delete;
    RS_Texture(RS_Texture&&);
    ~RS_Texture();

    RS_Texture& operator=(const RS_Texture&) = delete;
    RS_Texture& operator=(RS_Texture&&);

    void bind(GLint textureSlot) const;

    // Set wrapping mode for environment maps
    void setEnvironmentMapWrapping();

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getChannels() const { return m_channels; }
    bool isHDR() const { return m_isHDR; }

private:
    static constexpr GLuint INVALID = 0xFFFFFFFF;
    GLuint m_texture { INVALID };
    int m_width { 0 };
    int m_height { 0 };
    int m_channels { 0 };
    bool m_isHDR { false };
};
