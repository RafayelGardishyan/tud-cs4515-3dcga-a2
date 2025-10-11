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
    // Draw the entire scene with multi-pass lighting
    void draw(const Shader& drawShader);

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

    // Cameras
    std::vector<std::unique_ptr<Trackball>> m_cameras;
    int m_activeCameraIndex { 0 };
};


#endif //COMPUTERGRAPHICS_RSSCENE_H