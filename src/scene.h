//
// Created by Rafayel on 10/11/2025.
//

#ifndef COMPUTERGRAPHICS_RSSCENE_H
#define COMPUTERGRAPHICS_RSSCENE_H
#include <vector>

#include "light.h"
#include "model.h"
#include "framework/trackball.h"
#include "framework/shader.h"

class RS_Scene
{
public:
    RS_Scene();
    ~RS_Scene();

    // Delete copy constructor and copy assignment (scenes shouldn't be copied)
    RS_Scene(const RS_Scene&) = delete;
    RS_Scene& operator=(const RS_Scene&) = delete;

    // Move constructor and move assignment
    RS_Scene(RS_Scene&& other) noexcept;
    RS_Scene& operator=(RS_Scene&& other) noexcept;

    // Draw the entire scene with multi-pass lighting
    void draw(const Shader& drawShader, bool debugLights);

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

private:
    // Models (meshes + materials + transforms)
    std::vector<RS_Model> m_models;

    // Lights
    std::vector<RS_Light> m_lights;
    size_t m_selectedLightIndex = 0;
    Shader m_lightShader;
    unsigned int m_lightVAO = 0;

    // Cameras
    std::vector<std::unique_ptr<Trackball>> m_cameras;
    int m_activeCameraIndex { 0 };
};


#endif //COMPUTERGRAPHICS_RSSCENE_H