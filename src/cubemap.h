#pragma once
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()
#include <exception>
#include <filesystem>
#include <framework/opengl_includes.h>
#include <memory>

// Forward declaration
class RS_Texture;
class Shader;

class RS_Cubemap {
public:
    // Create a cubemap from an equirectangular HDR texture
    RS_Cubemap(const RS_Texture& equirectTexture, int resolution = 512);

    RS_Cubemap(const RS_Cubemap&) = delete;
    RS_Cubemap(RS_Cubemap&&);
    ~RS_Cubemap();

    RS_Cubemap& operator=(const RS_Cubemap&) = delete;
    RS_Cubemap& operator=(RS_Cubemap&&);

    void bind(GLint textureSlot);

    int getResolution() const { return m_resolution; }

private:
    static constexpr GLuint INVALID = 0xFFFFFFFF;
    GLuint m_cubemap { INVALID };
    int m_resolution { 512 };

    void convertEquirectToCubemap(const RS_Texture& equirectTexture);
};