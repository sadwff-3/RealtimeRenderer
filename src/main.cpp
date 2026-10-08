#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#include <filesystem>
#include <string>

#include"Shader.h"
#include"Texture.h"
#include"Camera.h"
#include"Material.h"
#include"Mesh.h"
#include"Model.h"
#include"Light.h"
#include"Transform.h"
#include"SceneObject.h"
#include"Scene.h"
#include"AssetManager.h"
#include"Renderer.h"
#include"EnvironmentMap.h"

#include<glad/gl.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<vector>
#include<utility>
#include <iostream>
#include<imgui.h>
#include<imgui_internal.h>
#include<imgui_impl_glfw.h>
#include<imgui_impl_opengl3.h>
#include<ImGuizmo.h>


//Camera
Camera camera;
float lastX = 640.0f;
float lastY = 360.0f;
bool firstMouse = true;
bool viewportHovered = false;
bool cameraMouseLocked = false;
bool editMode = false;
ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;

//framebuffer size
int framebufferWidth = 1280;
int framebufferHeight = 720;
bool framebufferResized = false;

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!cameraMouseLocked)
    {
        firstMouse = true;
        return;
    }

    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos);

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);
    
    camera.processMouseMovement(xoffset, yoffset);
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button != GLFW_MOUSE_BUTTON_RIGHT || action != GLFW_PRESS)
        return;

    if (cameraMouseLocked)
    {
        cameraMouseLocked = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse = true;
        return;
    }

    if (viewportHovered)
    {
        cameraMouseLocked = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    framebufferWidth = width;
    framebufferHeight = height;
    framebufferResized = true;
    glViewport(0, 0, width, height);
}


void scroll_callback(GLFWwindow* window,double xoffset,double yoffset)
{
    if (!cameraMouseLocked && !viewportHovered)
    {
        return;
    }

    camera.processMouseScroll(static_cast<float>(yoffset));
}


std::string openModelFileDialog()
{
#ifdef _WIN32

    char filePath[MAX_PATH] = {};

    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;

    dialog.lpstrFilter = "glTF Models (*.gltf;*.glb)\0" "*.gltf;*.glb\0" "All Files (*.*)\0" "*.*\0";

    dialog.nFilterIndex = 1;

    dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&dialog))
    {
        return std::string(filePath);
    }

#endif

    return {};
}


bool keyPressedOnce(GLFWwindow* window, int key)
{
    static bool previousStates[GLFW_KEY_LAST + 1] = {};
    bool current = glfwGetKey(window, key) == GLFW_PRESS;
    bool pressed = current && !previousStates[key];
    previousStates[key] = current;
    return pressed;
}


void focusSelectedObject(Camera& camera, SceneObject* object, float aspectRatio)
{
    if (object == nullptr)
    {
        return;
    }

    AABB bounds;

    if (!object->getWorldBounds(bounds))
    {
        return;
    }

    glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
    glm::vec3 size = bounds.max - bounds.min;
    float radius = glm::length(size) * 0.5;

    if (radius < 0.001f)
    {
        radius = 1.0f;
    }
    float verticalFov = glm::radians(camera.Zoom);
    float horizontalFov = 2.0f * atan(tan(verticalFov * 0.5f) * aspectRatio);
    float limitingFov = glm::min(verticalFov, horizontalFov);
    float distance = radius / tan(limitingFov * 0.5);

    distance *= 1.2f;

    camera.Position = center - camera.Front * distance;
}


int main(int argc, char** argv)
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Realtime Renderer", nullptr, nullptr);

    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    //GLAD
    int version = gladLoadGL(glfwGetProcAddress);
    
    if (version == 0)
    {
        std::cerr << "Failed to initialize GLAD\n";

        glfwDestroyWindow(window);
        glfwTerminate();

        return -1;
    }
    std::cout
        << "OpenGL: "
        << glGetString(GL_VERSION)
        << '\n';

    //Window¤ò¤Ä¤¯¤ë
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");
    
   
    //shader¥Õ¥¡¥¤¥ë¤òÕi¤ßÞz¤à
    {
        
        Shader shadowDebugShader("shaders/shadow_debug.vert", "shaders/shadow_debug.frag");
        
        AssetManager assets;
        Scene scene;

        SceneObject* selectedObject = nullptr;
        int selectedMaterialIndex = -1;

        Renderer renderer(framebufferWidth, framebufferHeight);
        EnvironmentMap environmentMap;
        Texture testHDR("assets/square/symmetrical_garden_4k.hdr");

        if (testHDR.valid())
        {
            environmentMap.generate(testHDR);
            environmentMap.generateIrradiance();
            environmentMap.generatePrefilter();
            environmentMap.generateBRDFLUT();
        }

        Model* apartment =
            assets.loadModel("assets/main_sponza/NewSponza_Main_glTF_003.gltf", true);

        if (apartment)
        {
            scene.addObject("Apartment", apartment);
        }
        else
        {
            std::cerr << "Failed to load Apartment\n";
        }

        //debug
        std::vector<Vertex> debugQuadVertices =
        {
            {{-1.0f, -1.0f, 0.0f}, {0,0,1}, {0.0f, 0.0f}},
            {{ 1.0f, -1.0f, 0.0f}, {0,0,1}, {1.0f, 0.0f}},
            {{ 1.0f,  1.0f, 0.0f}, {0,0,1}, {1.0f, 1.0f}},
            {{-1.0f,  1.0f, 0.0f}, {0,0,1}, {0.0f, 1.0f}}
        };

        std::vector<unsigned int> debugQuadIndices =
        {
            0, 1, 2,
            2, 3, 0
        };

        Mesh debugQuad(debugQuadVertices, debugQuadIndices);

        shadowDebugShader.use();
        shadowDebugShader.setInt("depthMap", 0);

        glEnable(GL_DEPTH_TEST);

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;
        int toneMappingMode = 1;
        int debugView = 0;

        int viewportRenderWidth = framebufferWidth;
        int viewportRenderHeight = framebufferHeight;
        
        //Lights
        
        Light directionalLight;
        directionalLight.type = LightType::Directional;
        directionalLight.color = glm::vec3(1.0f);
        directionalLight.intensity = 1.0f;
        directionalLight.castShadow = true;

        SceneObject& sun = scene.addLight("Directional Light", directionalLight);
        sun.transform.setEulerDegrees(glm::vec3(-29.0f, 59.0f, 0.0f));

        Light spotLight;
        spotLight.type = LightType::Spot;
        spotLight.color = glm::vec3(1.0f, 0.95f, 0.8f);
        spotLight.intensity = 15.0f;
        spotLight.range = 12.0f;
        spotLight.innerAngle = 25.0f;
        spotLight.outerAngle = 35.0f;
        spotLight.castShadow = true;

        SceneObject& spot = scene.addLight("Test Spotlight", spotLight);
        spot.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
        spot.transform.setEulerDegrees(glm::vec3(-90.0f, 0.0f, 0.0f));
      
        Light pointLight;
        pointLight.type = LightType::Point;
        pointLight.color = glm::vec3(1.0f);
        pointLight.intensity = 20.0f;
        pointLight.range = 10.0f;
        pointLight.castShadow = true;

        SceneObject& point = scene.addLight("Test Point Light", pointLight);
        point.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);


        while (!glfwWindowShouldClose(window))
        {
            float currentFrame = static_cast<float>(glfwGetTime());

            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            float cameraSpeed = 2.5 * deltaTime;

            if (keyPressedOnce(window, GLFW_KEY_ESCAPE))
            {
                if (cameraMouseLocked)
                {
                    cameraMouseLocked = false;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    firstMouse = true;
                }
                else
                {
                    glfwSetWindowShouldClose(window, true);
                }
            }

            if (!cameraMouseLocked && editMode && viewportHovered)
            {
                if (keyPressedOnce(window, GLFW_KEY_W))
                {
                    gizmoOperation = ImGuizmo::TRANSLATE;
                }

                if (keyPressedOnce(window, GLFW_KEY_E))
                {
                    gizmoOperation = ImGuizmo::ROTATE;
                }

                if (keyPressedOnce(window, GLFW_KEY_R))
                {
                    gizmoOperation = ImGuizmo::SCALE;
                }
            }

            if (cameraMouseLocked)
            {
                float moveDelta = deltaTime;

                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
                {
                    moveDelta *= 3.0f;
                }

                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                {
                    camera.processKeyboard(CameraMovement::Forward, moveDelta);
                }

                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                {
                    camera.processKeyboard(CameraMovement::Backward, moveDelta);
                }

                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                {
                    camera.processKeyboard(CameraMovement::Left, moveDelta);
                }

                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                {
                    camera.processKeyboard(CameraMovement::Right, moveDelta);
                }

                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                {
                    camera.processKeyboard(CameraMovement::Up, moveDelta);
                }

                if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
                {
                    camera.processKeyboard(CameraMovement::Down, moveDelta);
                }

                if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
                {
                    toneMappingMode = 0;
                }

                if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
                {
                    toneMappingMode = 1;
                }

                if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) debugView = 0;
                if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) debugView = 1;
                if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) debugView = 2;
                if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS) debugView = 3;
                if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) debugView = 4;
                if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS) debugView = 5;
                if (glfwGetKey(window, GLFW_KEY_F7) == GLFW_PRESS) debugView = 6;
                if (glfwGetKey(window, GLFW_KEY_F8) == GLFW_PRESS) debugView = 7;
            }
            if (framebufferWidth == 0 || framebufferHeight == 0)
            {
                glfwPollEvents();
                continue;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGuizmo::BeginFrame();
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Import Model..."))
                    {
                        std::string path = openModelFileDialog();
                        
                        if (!path.empty())
                        {
                            Model* model = assets.loadModel(path);
                            if (model != nullptr)
                            {
                                std::filesystem::path filePath(path);
                                SceneObject& object = scene.addObject(filePath.stem().string(), model);
                                selectedObject = &object;
                            }
                            else
                            {
                                std::cerr << "Failed to import model: " << path << '\n';
                            }
                        }
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Edit"))
                {
                    ImGui::MenuItem("Edit Mode", nullptr, &editMode);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Create"))
                {
                    if (ImGui::MenuItem("Directional Light"))
                    {
                        static int directionalLightCount = 1;

                        Light light;
                        light.type = LightType::Directional;
                        light.color = glm::vec3(1.0f);
                        light.intensity = 1.0f;
                        light.castShadow = false;

                        SceneObject& object = scene.addLight("Directional Light " + std::to_string(directionalLightCount++), light);
                        object.transform.setEulerDegrees(glm::vec3(-45.0f, 45.0f, 0.0f));

                        selectedObject = &object;
                    }

                    if (ImGui::MenuItem("Point Light"))
                    {
                        static int pointLightCount = 1;

                        Light light;
                        light.type = LightType::Point;
                        light.color = glm::vec3(1.0f);
                        light.intensity = 10.0f;
                        light.range = 10.0f;
                        light.castShadow = false;

                        SceneObject& object = scene.addLight("Point Light " + std::to_string(pointLightCount++), light);

                        object.transform.position = glm::vec3(0.0f, 2.0f, 0.0f);

                        selectedObject = &object;
                    }

                    if (ImGui::MenuItem("Spot Light"))
                    {
                        static int spotLightCount = 1;

                        Light light;
                        light.type = LightType::Spot;
                        light.color = glm::vec3(1.0f);
                        light.intensity = 10.0f;
                        light.range = 10.0f;
                        light.innerAngle = 20.0f;
                        light.outerAngle = 30.0f;
                        light.castShadow = false;

                        SceneObject& object = scene.addLight("Spot Light " + std::to_string(spotLightCount++), light);

                        object.transform.position = glm::vec3(0.0f, 2.0f, 0.0f);
                        object.transform.setEulerDegrees(glm::vec3(-45.0f, 0.0f, 0.0f));

                        selectedObject = &object;
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }   
            
         //DirectionalShadowPass
            renderer.renderDirectionalShadowPass(scene, camera);

         //SpotShadowPass
            renderer.renderSpotShadowPass(scene);

         //PointShadowPass
            renderer.renderPointShadowPass(scene);

         //GeometryPass
            renderer.renderGeometryPass(scene, camera);

         //SsaoPass
            renderer.renderSSAOPass(debugQuad, camera);
            renderer.renderSSAOBlurPass(debugQuad);

         //DeferredLightingPass
            renderer.renderDeferredLightingPass(debugQuad, camera, scene, debugView, environmentMap.getCubemap(), environmentMap.getIrradianceMap(), environmentMap.getPrefilterMap(), environmentMap.getBRDFLUT());

         //ScreenPass
            renderer.renderScreenPass(debugQuad, toneMappingMode, debugView);
            
           

            if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
            {
                glDisable(GL_DEPTH_TEST);

                shadowDebugShader.use();

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, renderer.getShadowMap());

                debugQuad.draw();

                glEnable(GL_DEPTH_TEST);
            }
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags dockspaceFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            ImGui::Begin("EditorDockSpace", nullptr, dockspaceFlags);
            ImGui::PopStyleVar(3);

            ImGuiID dockspaceID = ImGui::GetID("EditorDockSpaceID");

            static bool dockLayoutBuilt = false;

            if (!dockLayoutBuilt)
            {
                dockLayoutBuilt = true;

                ImGui::DockBuilderRemoveNode(dockspaceID);

                ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);

                ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

                ImGuiID dockMainID = dockspaceID;

                ImGuiID dockLeftID = ImGui::DockBuilderSplitNode(dockMainID, ImGuiDir_Left, 0.20f, nullptr, &dockMainID);

                ImGuiID dockRightID = ImGui::DockBuilderSplitNode(dockMainID, ImGuiDir_Right, 0.25f, nullptr, &dockMainID);

                ImGui::DockBuilderDockWindow("Hierarchy", dockLeftID);

                ImGui::DockBuilderDockWindow("Inspector", dockRightID);

                ImGui::DockBuilderDockWindow("Viewport", dockMainID);

                ImGui::DockBuilderFinish(dockspaceID);
            }

            ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

            ImGui::End();

            SceneObject* objectToDelete = nullptr;
            ImGui::Begin("Hierarchy");
            for (const auto& object : scene.getObjects())
            {
                ImGui::PushID(object.get());
                bool selected = (selectedObject == object.get());

                if (ImGui::Selectable(object->name.c_str(), selected))
                {
                    selectedObject = object.get();
                    selectedMaterialIndex = -1;

                    if (editMode && !selectedObject->light.has_value() && viewportRenderWidth > 0 && viewportRenderHeight > 0)
                    {
                        float aspectRatio = static_cast<float>(viewportRenderWidth) / static_cast<float>(viewportRenderHeight);
                        focusSelectedObject(camera, selectedObject, aspectRatio);
                    }
                }
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Delete"))
                    {
                        objectToDelete = object.get();
                    }

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
            ImGui::End();
            if (selectedObject != nullptr && ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive())
            {
                objectToDelete = selectedObject;
            }

            if (objectToDelete != nullptr)
            {
                if (selectedObject == objectToDelete)
                {
                    selectedObject = nullptr;
                    selectedMaterialIndex = -1;
                }

                scene.removeObject(objectToDelete);
            }

            ImGui::Begin("Viewport");
            viewportHovered = ImGui::IsWindowHovered();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            
            int newViewportWidth = static_cast<int>(viewportSize.x);
            int newViewportHeight = static_cast<int>(viewportSize.y);

            if (newViewportWidth > 0 && newViewportHeight > 0 && (newViewportWidth != viewportRenderWidth || newViewportHeight != viewportRenderHeight))
            {
                viewportRenderWidth = newViewportWidth;
                viewportRenderHeight = newViewportHeight;
                
                renderer.resizeSceneFramebuffer(viewportRenderWidth, viewportRenderHeight);
            }

            ImVec2 imageMin = ImGui::GetCursorScreenPos();

            ImGui::Image((ImTextureID)(intptr_t)renderer.getFinalColorTexture(), viewportSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            
            if (selectedObject != nullptr && editMode && viewportSize.x > 0.0f && viewportSize.y > 0.0f && !cameraMouseLocked)
            {
                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                ImGuizmo::SetRect(imageMin.x, imageMin.y, viewportSize.x, viewportSize.y);
                
                glm::mat4 view = camera.getViewMatrix();
                glm::mat4 projection = camera.getProjectionMatrix(viewportSize.x / viewportSize.y);
                glm::mat4 model = selectedObject->transform.getMatrix();

                ImGuizmo::MODE gizmoMode = gizmoOperation == ImGuizmo::SCALE ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

                ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), gizmoOperation, gizmoMode, glm::value_ptr(model));

                if (ImGuizmo::IsUsing())
                {
                    selectedObject->transform.setFromMatrix(model);
                }
                
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !cameraMouseLocked && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
            {
                ImVec2 mousePos = ImGui::GetMousePos();
                float localX = mousePos.x - imageMin.x;
                float localY = mousePos.y - imageMin.y;
                int pixelX = static_cast<int>(localX / viewportSize.x * viewportRenderWidth);
                int pixelY = viewportRenderHeight - 1 - static_cast<int>(localY / viewportSize.y * viewportRenderHeight);
                unsigned int objectID = renderer.readObjectID(pixelX, pixelY);
                unsigned int materialID = renderer.readMaterialID(pixelX, pixelY);
                const auto& objects = scene.getObjects();

                if (objectID > 0 && objectID <= objects.size())
                {
                    selectedObject = objects[objectID - 1].get();
                    selectedMaterialIndex = -1;

                    if (selectedObject->model != nullptr && materialID > 0)
                    {
                        std::size_t index = static_cast<std::size_t>(materialID - 1);

                        if (index < selectedObject->model->getMaterialCount())
                        {
                            selectedMaterialIndex = static_cast<int>(index);
                        }
                    }
                }
                else
                {
                    selectedObject = nullptr;
                    selectedMaterialIndex = -1;
                }
            }

            ImGui::End();

            ImGui::Begin("Inspector");
            if (selectedObject != nullptr)
            {
                ImGui::TextUnformatted(selectedObject->name.c_str());
                ImGui::Separator();
                ImGui::TextUnformatted("Transform");
                ImGui::DragFloat3("Position", &selectedObject->transform.position.x, 0.05f);
                glm::vec3 eulerDegrees = selectedObject->transform.getEulerDegrees();

                if (ImGui::DragFloat3("Rotation", &eulerDegrees.x, 0.5f))
                {
                    selectedObject->transform.setEulerDegrees(eulerDegrees);
                }

                ImGui::DragFloat3("Scale", &selectedObject->transform.scale.x, 0.05f);

                if (selectedObject->light.has_value())
                {
                    Light& light = selectedObject->light.value();

                    ImGui::Separator();
                    ImGui::TextUnformatted("Light");

                    ImGui::ColorEdit3("Color", &light.color.x);
                    ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
                    
                    if (light.type == LightType::Point || light.type == LightType::Spot)
                    {
                        ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 100.0f);
                    }

                    if (light.type == LightType::Spot)
                    {
                        ImGui::DragFloat("Inner Angle", &light.innerAngle, 0.5f, 0.0f, 90.0f);
                        ImGui::DragFloat("Outer Angle", &light.outerAngle, 0.5f, 0.0f, 90.0f);
                    }
                    ImGui::Checkbox("Cast Shadow", &light.castShadow);
                }

                if (selectedObject->model != nullptr)
                {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Material");

                    std::size_t materialCount = selectedObject->model->getMaterialCount();
                    std::string materialPreview = "None";

                    if (selectedMaterialIndex >= 0 && static_cast<std::size_t>(selectedMaterialIndex) < materialCount)
                    {
                        materialPreview = "Materail" + std::to_string(selectedMaterialIndex);
                    }
                    if (ImGui::BeginCombo("Material Slot", materialPreview.c_str()))
                    {
                        for (std::size_t i = 0; i < materialCount; ++i)
                        {
                            bool selected = selectedMaterialIndex == static_cast<int>(i);
                            std::string label = "Material" + std::to_string(i);

                            if (ImGui::Selectable(label.c_str(), selected))
                            {
                                selectedMaterialIndex = static_cast<int>(i);
                            }

                            if (selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }

                        ImGui::EndCombo();
                    }

                    if (selectedMaterialIndex >= 0 && static_cast<std::size_t>(selectedMaterialIndex) < materialCount)
                    {
                        Material& material = selectedObject->model->getMaterial(static_cast<std::size_t>(selectedMaterialIndex));

                        ImGui::ColorEdit4("Base Color", &material.baseColorFactor.x);
                        ImGui::SliderFloat("Metallic", &material.metallicFactor, 0.0f, 1.0f);
                        ImGui::SliderFloat("Roughness", &material.roughnessFactor, 0.0f, 1.0f);
                        ImGui::Checkbox("Double Sided", &material.doubleSided);
                        ImGui::Combo("Alpha Mode", &material.alphaMode, "Opaque\0Mask\0Blend (unsupported)\0");

                        if (material.alphaMode == 1)
                        {
                            ImGui::SliderFloat("Alpha Cutoff", &material.alphaCutoff, 0.0f, 1.0f);
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("Click a surface in the Viewport to select its material.");
                    }
                }

                ImGui::Separator();

                if (ImGui::Button("Delete Object"))
                {
                    SceneObject* objectToDelete = selectedObject;

                    selectedObject = nullptr;
                    selectedMaterialIndex = -1;

                    scene.removeObject(objectToDelete);
                }
            }
            else
            {
                ImGui::TextUnformatted("No object selected");
            }
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}