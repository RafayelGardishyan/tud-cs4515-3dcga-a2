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
#include <glm/gtc/type_ptr.hpp>
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

        m_window.registerWindowResizeCallback([this](const glm::ivec2& newSize) {
            glViewport(0, 0, newSize.x, newSize.y);
        });

        // Set initial viewport size
        glm::ivec2 windowSize = m_window.getWindowSize();
        glViewport(0, 0, windowSize.x, windowSize.y);

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "Shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

            ShaderBuilder lightBuilder;
            lightBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/light_vert.glsl");
            lightBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/light_frag.glsl");
            m_lightShader = lightBuilder.build();

            ShaderBuilder envBuilder;
            envBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/env_vert.glsl");
            envBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/env_frag.glsl");
            m_envShader = envBuilder.build();

            ShaderBuilder skyboxBuilder;
            skyboxBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/skybox_vert.glsl");
            skyboxBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/skybox_frag.glsl");
            m_skyboxShader = skyboxBuilder.build();

        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }

        glGenVertexArrays(1, &m_lightVAO);

        // Create skybox cube VAO
        float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        GLuint skyboxVBO;
        glGenVertexArrays(1, &m_skyboxVAO);
        glGenBuffers(1, &skyboxVBO);
        glBindVertexArray(m_skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        // Create default scene
        RS_Scene defaultScene;

        // Add default light to the scene
        RS_Light defaultLight;
        defaultLight.m_position = glm::vec3(0.0f, 10.0f, 10.0f);
        defaultLight.m_color = glm::vec3(1.0f, 1.0f, 1.0f);
        defaultLight.m_intensity = 1.0f;
        defaultScene.addLight(defaultLight);

        // Add default camera to the scene
        defaultScene.addCamera(std::make_unique<Trackball>(&m_window, glm::radians(80.0f), 4.0f));

        // Load dragon model
        RS_Model shipModel;
        std::vector<GPUMesh> shipMeshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/ship/ship.obj", true);

        // Add all meshes to the model
        for (GPUMesh& mesh : shipMeshes) {
            // Create material from mesh data (automatically loads textures)
            RS_Material material = RS_Material::createFromMesh(mesh);

            shipModel.addMesh(std::move(mesh));
            shipModel.addMaterial(std::move(material));
        }

        // Add dragon to scene
        defaultScene.addModel(std::move(shipModel));

        // Add environment map to scene
        defaultScene.setEnvironmentMap(RESOURCE_ROOT "resources/envmap/pure_sky.hdr");

        // Add the default scene to the scenes
        m_scenes.push_back(std::move(defaultScene));

        Trackball::printHelp(); // Print camera controls to console
    }

    void render_imgui()
    {
        if (m_scenes.empty()) return;

        RS_Scene& activeScene = m_scenes[m_activeSceneIndex];

        ImGui::Begin(RS_WINDOW_TITLE);

        // Scene controls
        ImGui::Separator();
        ImGui::Text("Scenes");
        ImGui::Text("Active Scene: %d / %zu", m_activeSceneIndex + 1, m_scenes.size());
        if (ImGui::Button("Previous Scene") && m_activeSceneIndex > 0) {
            m_activeSceneIndex--;
        }
        ImGui::SameLine();
        if (ImGui::Button("Next Scene") && m_activeSceneIndex < m_scenes.size() - 1) {
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

        if (ImGui::BeginListBox("##lights", ImVec2(-FLT_MIN, 4 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (size_t i = 0; i < lights.size(); i++) {
                const bool isSelected = (i == static_cast<size_t>(m_selectedLightIndex));
                if (ImGui::Selectable(("Light " + std::to_string(i + 1)).c_str(), isSelected)) {
                    m_selectedLightIndex = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }

        if (ImGui::Button("Add Light")) {
            RS_Light newLight;
            newLight.m_position = glm::vec3(0.0f, 2.0f, 0.0f);
            newLight.m_color = glm::vec3(1.0f, 1.0f, 1.0f);
            newLight.m_intensity = 1.0f;
            activeScene.addLight(newLight);
        }
        if (!lights.empty()) {
            RS_Light& selectedLight = lights[m_selectedLightIndex];
            ImGui::DragFloat3("Position", glm::value_ptr(selectedLight.m_position), 0.1f);
            ImGui::ColorEdit3("Color", glm::value_ptr(selectedLight.m_color));
            ImGui::DragFloat("Intensity", &selectedLight.m_intensity, 0.1f, 0.0f, 100.0f);
        }

        // Environment Map controls
        ImGui::Separator();
        ImGui::Text("Environment Map");
        if (activeScene.getEnvironmentCubemap()) {
            float& envBrightness = activeScene.getEnvironmentBrightness();
            ImGui::SliderFloat("Brightness", &envBrightness, 0.0f, 2.0f);
        } else {
            ImGui::TextDisabled("No environment map loaded");
        }

        // Global Texture Toggles
        ImGui::Separator();
        ImGui::Text("Global Texture Toggles");
        ImGui::Checkbox("Enable Color Textures", &m_settings.enableColorTextures);
        ImGui::Checkbox("Enable Normal Textures", &m_settings.enableNormalTextures);
        ImGui::Checkbox("Enable Metallic Textures", &m_settings.enableMetallicTextures);

        ImGui::Separator();
        ImGui::Text("Color correction");
        ImGui::Checkbox("Enable gamma correction", &m_settings.enableGammaCorrection);
        ImGui::Checkbox("Enable tone mapping", &m_settings.enableToneMapping);

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

    void draw_lights(RS_Scene& activeScene)
    {
        // Render debug lights if enabled
        if (m_debug && !activeScene.getLights().empty()) {
            glDisable(GL_DEPTH_TEST);

            const Trackball& camera = activeScene.getActiveCamera();
            const glm::mat4 viewMatrix = camera.viewMatrix();
            const glm::mat4 projectionMatrix = camera.projectionMatrix();
            const glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

            m_lightShader.bind();

            // Render selected light with yellow color and larger size
            {
                const std::vector<RS_Light>& lights = activeScene.getLights();
                const glm::vec4 screenPos = viewProjectionMatrix * glm::vec4(lights[m_selectedLightIndex].m_position, 1.0f);
                const glm::vec3 color{1, 1, 0};

                glPointSize(40.0f);
                glUniform4fv(m_lightShader.getUniformLocation("pos"), 1, glm::value_ptr(screenPos));
                glUniform3fv(m_lightShader.getUniformLocation("color"), 1, glm::value_ptr(color));
                glBindVertexArray(m_lightVAO);
                glDrawArrays(GL_POINTS, 0, 1);
                glBindVertexArray(0);
            }

            // Render all lights with their colors
            for (const RS_Light& light : activeScene.getLights()) {
                const glm::vec4 screenPos = viewProjectionMatrix * glm::vec4(light.m_position, 1.0f);

                glPointSize(10.0f);
                glUniform4fv(m_lightShader.getUniformLocation("pos"), 1, glm::value_ptr(screenPos));
                glUniform3fv(m_lightShader.getUniformLocation("color"), 1, glm::value_ptr(light.m_color));
                glBindVertexArray(m_lightVAO);
                glDrawArrays(GL_POINTS, 0, 1);
                glBindVertexArray(0);
            }
        }
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
                RS_Scene& activeScene = m_scenes[m_activeSceneIndex];

                // Draw skybox background first
                activeScene.drawSkybox(m_skyboxShader, m_skyboxVAO);

                // Draw environment map
                activeScene.drawEnvironment(m_envShader, m_settings);

                // Draw direct lighting
                activeScene.draw(m_defaultShader, m_settings);

                // Draw debug lights
                draw_lights(activeScene);
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

    // Shaders
    Shader m_defaultShader;
    Shader m_shadowShader;
    Shader m_lightShader;
    Shader m_envShader;
    Shader m_skyboxShader;

    unsigned int m_lightVAO = 0;
    unsigned int m_skyboxVAO = 0;

    size_t m_selectedLightIndex = 0;

    bool m_debug = true;

    // Global texture toggles
    RS_RenderSettings m_settings;

    // Scene system
    std::vector<RS_Scene> m_scenes;
    size_t m_activeSceneIndex { 0 };
};

int main()
{
    Application app;
    app.update();

    return 0;
}
