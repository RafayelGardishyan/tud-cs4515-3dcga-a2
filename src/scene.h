//
// Created by Rafayel on 10/11/2025.
//

#ifndef COMPUTERGRAPHICS_RSSCENE_H
#define COMPUTERGRAPHICS_RSSCENE_H
#include <utility>
#include <vector>

#include "light.h"
#include "model.h"
#include "texture.h"
#include "cubemap.h"
#include "framework/trackball.h"
#include "framework/shader.h"

struct RS_RenderSettings
{
    bool enableColorTextures = true;
    bool enableNormalTextures = true;
    bool enableMetallicTextures = true;
    bool enableToneMapping = true;
    bool enableGammaCorrection = true;
};

class RS_Scene
{
public:
    RS_Scene() = default;

    // Draw the entire scene
    void draw(const Shader& drawShader, RS_RenderSettings settings);

    // Draw environment map contribution
    void drawEnvironment(const Shader& envShader, RS_RenderSettings settings);

    // Draw skybox background
    void drawSkybox(const Shader& skyboxShader, GLuint skyboxVAO);

    // These functions are called by the application
    void onKeyPressed(int key, int mods) {};
    void onKeyReleased(int key, int mods) {};
    void onMouseMove(const glm::dvec2& cursorPos) {};
    void onMouseClicked(int button, int mods) {};
    void onMouseReleased(int button, int mods) {};

    // Camera management
    Trackball& getActiveCamera() { return *m_cameras[m_activeCameraIndex]; }
    const Trackball& getActiveCamera() const { return *m_cameras[m_activeCameraIndex]; }
    std::vector<std::unique_ptr<Trackball>>& getCameras() { return m_cameras; }
    const std::vector<std::unique_ptr<Trackball>>& getCameras() const { return m_cameras; }
    int getActiveCameraIndex() const { return m_activeCameraIndex; }
    void setActiveCameraIndex(int index) { m_activeCameraIndex = index; }
    void addCamera(std::unique_ptr<Trackball> camera) { m_cameras.push_back(std::move(camera)); }

    // Light management
    std::vector<RS_Light>& getLights() { return m_lights; }
    const std::vector<RS_Light>& getLights() const { return m_lights; }
    void addLight(const RS_Light& light) { m_lights.push_back(light); }

    // Model management
    std::vector<RS_Model>& getModels() { return m_models; }
    const std::vector<RS_Model>& getModels() const { return m_models; }
    void addModel(RS_Model&& model) { m_models.push_back(std::move(model)); }
    size_t getModelCount() const { return m_models.size(); }

    // Environment map management
    RS_Cubemap* getEnvironmentCubemap() { return m_environmentCubemap.get(); }
    const RS_Cubemap* getEnvironmentCubemap() const { return m_environmentCubemap.get(); }
    void setEnvironmentMap(std::filesystem::path filePath, bool isHDR = true, int cubemapResolution = 512) {
        // Load the equirectangular texture
        auto equirectTexture = std::make_unique<RS_Texture>(std::move(filePath), isHDR);
        equirectTexture->setEnvironmentMapWrapping();

        // Convert to cubemap
        m_environmentCubemap = std::make_unique<RS_Cubemap>(*equirectTexture, cubemapResolution);
    }

    // Environment brightness control
    float& getEnvironmentBrightness() { return m_envBrightness; }
    float getEnvironmentBrightness() const { return m_envBrightness; }
    void setEnvironmentBrightness(float brightness) { m_envBrightness = brightness; }

private:
    // Models (meshes + materials + transforms)
    std::vector<RS_Model> m_models;

    // Lights
    std::vector<RS_Light> m_lights;

    //Environment map
    std::unique_ptr<RS_Cubemap> m_environmentCubemap { nullptr };
    float m_envBrightness { 0.5f }; // Default brightness

    // Cameras
    std::vector<std::unique_ptr<Trackball>> m_cameras;
    int m_activeCameraIndex { 0 };
};


#endif //COMPUTERGRAPHICS_RSSCENE_H