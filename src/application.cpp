//#include "Image.h"
#include "constants.h"
#include "scene.h"
// Always include window first (because it includes glfw, which includes GL which needs to be included AFTER glew).
// Can't wait for modules to fix this stuff...
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
// Include glad before glfw3
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/trackball.h>
#include <framework/window.h>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

class Application {
public:
    Application()
        : m_window(RS_WINDOW_TITLE, RS_WINDOW_SIZE, OpenGLVersion::GL41)
    {
        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
        });

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "Shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

            // Any new shaders can be added below in similar fashion.
            // ==> Don't forget to reconfigure CMake when you do!
            //     Visual Studio: PROJECT => Generate Cache for ComputerGraphics
            //     VS Code: ctrl + shift + p => CMake: Configure => enter
            // ....
        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }

        // Create default scene
        RS_Scene defaultScene;

        // Add default light to the scene
        RS_Light defaultLight;
        defaultLight.position = glm::vec3(0.0f, 2.0f, 0.0f);
        defaultLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
        defaultLight.intensity = 1.0f;
        defaultScene.addLight(defaultLight);

        // Add default camera to the scene
        defaultScene.addCamera(std::make_unique<Trackball>(&m_window, glm::radians(80.0f), 4.0f));

        // Load dragon model
        RS_Model dragonModel;
        std::vector<GPUMesh> dragonMeshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/dragon.obj", true);

        // Add all meshes to the model
        for (GPUMesh& mesh : dragonMeshes) {
            dragonModel.addMesh(std::move(mesh));

            // Create a default material for each mesh
            RS_Material defaultMaterial;
            defaultMaterial.gpuData.baseColor = glm::vec3(0.8f, 0.8f, 0.8f);
            defaultMaterial.gpuData.metallic = 0.0f;
            defaultMaterial.gpuData.roughness = 0.5f;
            defaultMaterial.gpuData.transmission = 0.0f;
            defaultMaterial.gpuData.emissive = glm::vec3(0.0f);
            defaultMaterial.gpuData.textureFlags = glm::ivec4(0);
            dragonModel.addMaterial(defaultMaterial);
        }

        // Add dragon to scene
        defaultScene.addModel(std::move(dragonModel));

        // Add the default scene to the scenes
        m_scenes.push_back(std::move(defaultScene));

        Trackball::printHelp(); // Print camera controls to console
    }

    void render_imgui()
    {
        if (m_scenes.empty()) return;

        RS_Scene& activeScene = m_scenes[m_activeSceneIndex];

        ImGui::Begin("Window");

        // Scene controls
        ImGui::Separator();
        ImGui::Text("Scenes");
        ImGui::Text("Active Scene: %d / %zu", m_activeSceneIndex + 1, m_scenes.size());
        if (ImGui::Button("Previous Scene") && m_activeSceneIndex > 0) {
            m_activeSceneIndex--;
        }
        ImGui::SameLine();
        if (ImGui::Button("Next Scene") && m_activeSceneIndex < static_cast<int>(m_scenes.size()) - 1) {
            m_activeSceneIndex++;
        }

        // Camera controls
        ImGui::Separator();
        ImGui::Text("Cameras");
        int activeCameraIndex = activeScene.getActiveCameraIndex();
        size_t cameraCount = activeScene.getCameras().size();
        ImGui::Text("Active Camera: %d / %zu", activeCameraIndex + 1, cameraCount);

        if (ImGui::BeginListBox("##cameras", ImVec2(-FLT_MIN, 4 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (size_t i = 0; i < cameraCount; i++) {
                const bool isSelected = (i == static_cast<size_t>(activeCameraIndex));
                if (ImGui::Selectable(("Camera " + std::to_string(i + 1)).c_str(), isSelected)) {
                    activeCameraIndex = static_cast<int>(i);
                    activeScene.setActiveCameraIndex(activeCameraIndex);
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }

        // Lighting controls
        ImGui::Separator();
        ImGui::Text("Lights");
        std::vector<RS_Light>& lights = activeScene.getLights();
        ImGui::Text("Total Lights: %zu", lights.size());

        size_t selected_light_index = 0;

        if (ImGui::BeginListBox("##lights", ImVec2(-FLT_MIN, 4 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (size_t i = 0; i < lights.size(); i++) {
                const bool isSelected = (i == static_cast<size_t>(activeCameraIndex));
                if (ImGui::Selectable(("Light " + std::to_string(i + 1)).c_str(), isSelected)) {
                    selected_light_index = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }

        if (ImGui::Button("Add Light")) {
            RS_Light newLight;
            newLight.position = glm::vec3(0.0f, 2.0f, 0.0f);
            newLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
            newLight.intensity = 1.0f;
            activeScene.addLight(newLight);
        }
        if (!lights.empty()) {
            RS_Light& selectedLight = lights[selected_light_index];
            ImGui::DragFloat3("Position", glm::value_ptr(selectedLight.position), 0.1f);
            ImGui::ColorEdit3("Color", glm::value_ptr(selectedLight.color));
            ImGui::DragFloat("Intensity", &selectedLight.intensity, 0.1f, 0.0f, 100.0f);
        }

        // Model and Material controls
        ImGui::Separator();
        ImGui::Text("Models & Materials");
        std::vector<RS_Model>& models = activeScene.getModels();
        ImGui::Text("Total Models: %zu", models.size());

        for (size_t modelIdx = 0; modelIdx < models.size(); modelIdx++) {
            RS_Model& model = models[modelIdx];
            ImGui::PushID(static_cast<int>(modelIdx));

            if (ImGui::TreeNode(("Model " + std::to_string(modelIdx + 1)).c_str())) {
                std::vector<RS_Material>& materials = model.getMaterials();
                size_t meshCount = model.getMeshCount();

                ImGui::Text("Meshes: %zu", meshCount);

                for (size_t meshIdx = 0; meshIdx < meshCount && meshIdx < materials.size(); meshIdx++) {
                    ImGui::PushID(static_cast<int>(meshIdx));

                    if (ImGui::TreeNode(("Mesh " + std::to_string(meshIdx + 1)).c_str())) {
                        RS_Material& material = materials[meshIdx];

                        ImGui::ColorEdit3("Base Color", glm::value_ptr(material.gpuData.baseColor));
                        ImGui::SliderFloat("Metallic", &material.gpuData.metallic, 0.0f, 1.0f);
                        ImGui::SliderFloat("Roughness", &material.gpuData.roughness, 0.0f, 1.0f);
                        ImGui::SliderFloat("Transmission", &material.gpuData.transmission, 0.0f, 1.0f);
                        ImGui::ColorEdit3("Emissive", glm::value_ptr(material.gpuData.emissive));

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::Checkbox("Debug", &m_debug);

        ImGui::End();
    }

    void update()
    {
        while (!m_window.shouldClose()) {
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();

            render_imgui();

            // Clear the screen
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glEnable(GL_DEPTH_TEST);

            // Draw the active scene
            if (!m_scenes.empty()) {
                m_scenes[m_activeSceneIndex].draw(m_defaultShader, m_debug);
            }

            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        std::cout << "Key pressed: " << key << std::endl;
        m_scenes[m_activeSceneIndex].onKeyPressed(key, mods);
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;
        m_scenes[m_activeSceneIndex].onKeyReleased(key, mods);
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
        m_scenes[m_activeSceneIndex].onMouseMove(cursorPos);
    }

    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        std::cout << "Pressed mouse button: " << button << std::endl;
        m_scenes[m_activeSceneIndex].onMouseClicked(button, mods);
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        std::cout << "Released mouse button: " << button << std::endl;
        m_scenes[m_activeSceneIndex].onMouseReleased(button, mods);
    }

private:
    Window m_window;

    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;

    bool m_debug = true;

    // Scene system
    std::vector<RS_Scene> m_scenes;
    int m_activeSceneIndex { 0 };
};

int main()
{
    Application app;
    app.update();

    return 0;
}
