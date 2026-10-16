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


std::string openFileDialog(const char* filter, const char* title)
{
#ifdef _WIN32

    char filePath[MAX_PATH] = {};

    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;

    dialog.lpstrFilter = filter;
    dialog.lpstrTitle = title;

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


void applyEditorStyle()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 17.0f;
    style.WindowPadding = ImVec2(16.0f, 14.0f);
    style.FramePadding = ImVec2(9.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 9.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.WindowMinSize = ImVec2(180.0f, 120.0f);
    style.WindowRounding = 5.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.ScrollbarSize = 12.0f;
    style.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.93f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.53f, 0.60f, 1.0f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.095f, 0.105f, 0.125f, 1.0f);
    colors[ImGuiCol_ChildBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.135f, 0.16f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.23f, 0.28f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.18f, 0.215f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.25f, 0.30f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.29f, 0.35f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.125f, 0.15f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.175f, 0.21f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_PopupBg];
    colors[ImGuiCol_Header] = ImVec4(0.17f, 0.20f, 0.24f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.28f, 0.33f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.35f, 0.39f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.19f, 0.23f, 0.28f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.32f, 0.38f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.24f, 0.39f, 0.43f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.75f, 0.73f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.32f, 0.65f, 0.65f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.43f, 0.82f, 0.79f, 1.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.14f, 0.17f, 1.0f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.19f, 0.23f, 0.28f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.32f, 0.37f, 1.0f);
    colors[ImGuiCol_TabDimmed] = colors[ImGuiCol_TitleBg];
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.16f, 0.19f, 0.23f, 1.0f);
    colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
    colors[ImGuiCol_DockingPreview] = ImVec4(0.35f, 0.75f, 0.73f, 0.3f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 0.55f, 0.56f, 0.4f);
}

bool beginInspectorProperties(const char* id)
{
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
        return false;
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void inspectorProperty(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
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

    //Window§Ú§ƒ§Ø§Î
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

    applyEditorStyle();
    const char* editorFontPath = "C:/Windows/Fonts/segoeui.ttf";
    if (std::filesystem::exists(editorFontPath))
        io.Fonts->AddFontFromFileTTF(editorFontPath, 17.0f);
    else
        io.Fonts->AddFontDefault();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");
    
   
    //shader•’•°•§•Î§Ú’i§ﬂﬁz§‡
    {
        
        Shader shadowDebugShader("shaders/shadow_debug.vert", "shaders/shadow_debug.frag");
        
        AssetManager assets;
        Scene scene;

        SceneObject* selectedObject = nullptr;
        int selectedMaterialIndex = -1;

        Renderer renderer(framebufferWidth, framebufferHeight);
        EnvironmentMap environmentMap;
        std::string environmentName;
        std::string hdrImportError;
        bool showHDRImportError = false;

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
        bool viewportFullscreen = false;

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

        while (!glfwWindowShouldClose(window))
        {
            float currentFrame = static_cast<float>(glfwGetTime());

            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            float cameraSpeed = 2.5 * deltaTime;

            if (keyPressedOnce(window, GLFW_KEY_F12) && !ImGui::GetIO().WantTextInput)
                viewportFullscreen = !viewportFullscreen;

            if (keyPressedOnce(window, GLFW_KEY_ESCAPE))
            {
                if (cameraMouseLocked)
                {
                    cameraMouseLocked = false;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    firstMouse = true;
                }
                else if (viewportFullscreen)
                {
                    viewportFullscreen = false;
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
                    moveDelta *= 10.0f;
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
                if (glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS) debugView = 8;
                if (glfwGetKey(window, GLFW_KEY_F10) == GLFW_PRESS) debugView = 9;
                if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS) debugView = 10;
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
                        std::string path = openFileDialog(
                            "glTF Models (*.gltf;*.glb)\0*.gltf;*.glb\0All Files (*.*)\0*.*\0",
                            "Import Model");
                        
                        if (!path.empty())
                        {
                            Model* model = assets.loadModel(path);
                            if (model != nullptr)
                            {
                                std::filesystem::path filePath(path);
                                SceneObject& object = scene.addObject(filePath.stem().string(), model);
                                selectedObject = &object;
                                selectedMaterialIndex = -1;
                                focusSelectedObject(camera, selectedObject,
                                    static_cast<float>(viewportRenderWidth) / viewportRenderHeight);
                            }
                            else
                            {
                                std::cerr << "Failed to import model: " << path << '\n';
                            }
                        }
                    }
                    if (ImGui::MenuItem("Import HDR..."))
                    {
                        const std::string path = openFileDialog(
                            "HDR Images (*.hdr)\0*.hdr\0", "Import HDR Environment");
                        if (!path.empty())
                        {
                            if (environmentMap.loadHDR(path))
                            {
                                environmentName = std::filesystem::path(path).filename().string();
                                renderer.invalidateTAAHistory();
                            }
                            else
                            {
                                hdrImportError = "Could not load the HDR image:\n" + path;
                                showHDRImportError = true;
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

                if (ImGui::BeginMenu("View"))
                {
                    ImGui::MenuItem("Fullscreen Viewport", "F12", &viewportFullscreen);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Rendering"))
                {
                    if (ImGui::MenuItem("SSAO", nullptr, renderer.isSSAOEnabled()))
                        renderer.setSSAOEnabled(!renderer.isSSAOEnabled());

                    if (ImGui::MenuItem("TAA", nullptr, renderer.isTAAEnabled()))
                        renderer.setTAAEnabled(!renderer.isTAAEnabled());

                    if (ImGui::BeginMenu("Point Lights"))
                    {
                        ImGui::SliderInt("Shadow Budget", &renderer.pointShadowBudget, 0, 4);
                        ImGui::TextUnformatted("0: lighting only; 1: recommended; 4: highest cost");
                        for (SceneObject* object : scene.getLights())
                        {
                            if (!object->light || object->light->type != LightType::Point) continue;
                            Light& light = *object->light;
                            ImGui::PushID(object);
                            ImGui::Separator();
                            ImGui::TextUnformatted(object->name.c_str());
                            ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                            ImGui::Checkbox("Cast Shadow", &light.castShadow);
                            ImGui::PopID();
                        }
                        ImGui::EndMenu();
                    }

                    ImGui::Checkbox("Bloom", &renderer.bloom.enabled);
                    ImGui::SliderFloat("Bloom Intensity", &renderer.bloom.intensity, 0.0f, 1.0f, "%.3f");
                    ImGui::DragFloat("Bloom Threshold (HDR)", &renderer.bloom.threshold, 0.05f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Bloom Soft Knee", &renderer.bloom.softKnee, 0.0f, 1.0f);

                    if (ImGui::MenuItem("Final image (F1)"))
                        debugView = 0;

                    if (ImGui::MenuItem("Emissive only (F10)"))
                        debugView = 9;

                    if (ImGui::MenuItem("Bloom only (F11)"))
                        debugView = 10;

                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }
         
            if (showHDRImportError)
            {
                ImGui::OpenPopup("HDR Import Failed");
                showHDRImportError = false;
            }
            if (ImGui::BeginPopupModal("HDR Import Failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("%s", hdrImportError.c_str());
                ImGui::TextUnformatted("The current environment has been kept.");
                if (ImGui::Button("OK", ImVec2(100.0f, 0.0f)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            renderer.beginTAAFrame(camera);
            
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
            int lightingDebugView = debugView == 10 ? 0 : debugView;
            renderer.renderDeferredLightingPass(debugQuad, camera, scene, lightingDebugView, environmentMap.getCubemap(), environmentMap.getIrradianceMap(), environmentMap.getPrefilterMap(), environmentMap.getBRDFLUT());

            renderer.renderTransparentPass(scene, camera, lightingDebugView, environmentMap.getIrradianceMap(), environmentMap.getPrefilterMap(), environmentMap.getBRDFLUT());

            renderer.renderTAAPass(debugQuad, lightingDebugView);

         //BloomPass
            renderer.renderBloomPass(debugQuad, debugView);

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

            const float statusHeight = ImGui::GetFontSize() + 12.0f;
            const ImVec2 dockSize(viewport->WorkSize.x, glm::max(viewport->WorkSize.y - statusHeight, 1.0f));

            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(dockSize);
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

                ImGui::DockBuilderSetNodeSize(dockspaceID, dockSize);

                ImGuiID dockMainID = dockspaceID;

                const float leftWidth = glm::clamp(dockSize.x * 0.14f, 210.0f, 260.0f);
                ImGuiID dockLeftID = ImGui::DockBuilderSplitNode(dockMainID, ImGuiDir_Left, leftWidth / dockSize.x, nullptr, &dockMainID);

                const float rightWidth = glm::clamp(dockSize.x * 0.22f, 350.0f, 410.0f);
                ImGuiID dockRightID = ImGui::DockBuilderSplitNode(dockMainID, ImGuiDir_Right, glm::min(rightWidth / (dockSize.x - leftWidth), 0.5f), nullptr, &dockMainID);

                ImGui::DockBuilderDockWindow("Hierarchy", dockLeftID);

                ImGui::DockBuilderDockWindow("Inspector", dockRightID);

                ImGui::DockBuilderDockWindow("Viewport", dockMainID);

                ImGui::DockBuilderFinish(dockspaceID);
            }

            ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

            ImGui::End();

            SceneObject* objectToDelete = nullptr;
            if (!viewportFullscreen)
            {
                ImGui::Begin("Hierarchy");
                ImGui::TextDisabled("SCENE  /  %zu OBJECTS", scene.getObjects().size());
                ImGui::Separator();
                ImGui::Spacing();
                for (const auto& object : scene.getObjects())
                {
                    ImGui::PushID(object.get());
                    bool selected = (selectedObject == object.get());

                    if (ImGui::Selectable(object->name.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(0.0f, ImGui::GetFrameHeight())))
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
            }
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

            bool viewportResizePending = false;
            ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_None;
            if (viewportFullscreen)
            {
                ImGui::SetNextWindowPos(viewport->WorkPos);
                ImGui::SetNextWindowSize(viewport->WorkSize);
                ImGui::SetNextWindowViewport(viewport->ID);
                viewportFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                viewportFullscreen ? ImVec2(0.0f, 0.0f) : ImVec2(6.0f, 6.0f));
            ImGui::Begin(viewportFullscreen ? "Viewport Fullscreen" : "Viewport", nullptr, viewportFlags);
            ImGui::PopStyleVar();
            viewportHovered = ImGui::IsWindowHovered();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            
            int newViewportWidth = static_cast<int>(viewportSize.x);
            int newViewportHeight = static_cast<int>(viewportSize.y);

            if (newViewportWidth > 0 && newViewportHeight > 0 && (newViewportWidth != viewportRenderWidth || newViewportHeight != viewportRenderHeight))
            {
                viewportRenderWidth = newViewportWidth;
                viewportRenderHeight = newViewportHeight;
                
                viewportResizePending = true;
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
                renderer.renderTransparentPickingPass(scene);

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

            if (!viewportFullscreen)
            {
                ImGui::Begin("Inspector");

                if (selectedObject != nullptr)
                {
                    ImGui::TextUnformatted(selectedObject->name.c_str());
                    ImGui::TextDisabled(selectedObject->light.has_value() ? "LIGHT OBJECT" : "MESH OBJECT");
                    ImGui::Spacing();

                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (beginInspectorProperties("TransformProperties"))
                        {
                            inspectorProperty("Position");
                            ImGui::DragFloat3("##Position", &selectedObject->transform.position.x, 0.05f, 0.0f, 0.0f, "%.2f");
                            glm::vec3 eulerDegrees = selectedObject->transform.getEulerDegrees();
                            inspectorProperty("Rotation");
                            if (ImGui::DragFloat3("##Rotation", &eulerDegrees.x, 0.5f, 0.0f, 0.0f, "%.1f"))
                                selectedObject->transform.setEulerDegrees(eulerDegrees);
                            inspectorProperty("Scale");
                            ImGui::DragFloat3("##Scale", &selectedObject->transform.scale.x, 0.05f, 0.0f, 0.0f, "%.2f");
                            ImGui::EndTable();
                        }
                    }

                    if (selectedObject->light.has_value() &&
                        ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        Light& light = selectedObject->light.value();
                        if (beginInspectorProperties("LightProperties"))
                        {
                            inspectorProperty("Color");
                            ImGui::ColorEdit3("##LightColor", &light.color.x, ImGuiColorEditFlags_NoInputs);
                            inspectorProperty("Intensity");
                            ImGui::DragFloat("##Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
                            if (light.type == LightType::Point || light.type == LightType::Spot)
                            {
                                inspectorProperty("Range");
                                ImGui::DragFloat("##Range", &light.range, 0.1f, 0.1f, 100.0f);
                            }
                            if (light.type == LightType::Spot)
                            {
                                inspectorProperty("Inner Angle");
                                ImGui::DragFloat("##InnerAngle", &light.innerAngle, 0.5f, 0.0f, 90.0f);
                                inspectorProperty("Outer Angle");
                                ImGui::DragFloat("##OuterAngle", &light.outerAngle, 0.5f, 0.0f, 90.0f);
                            }
                            inspectorProperty("Shadows");
                            ImGui::Checkbox("##CastShadow", &light.castShadow);
                            ImGui::EndTable();
                        }
                    }

                    if (selectedObject->model != nullptr &&
                        ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        const std::size_t materialCount = selectedObject->model->getMaterialCount();
                        std::string materialPreview = "Select a material";
                        if (selectedMaterialIndex >= 0 && static_cast<std::size_t>(selectedMaterialIndex) < materialCount)
                            materialPreview = "Material " + std::to_string(selectedMaterialIndex);

                        if (beginInspectorProperties("MaterialProperties"))
                        {
                            inspectorProperty("Slot");
                            if (ImGui::BeginCombo("##MaterialSlot", materialPreview.c_str()))
                            {
                                for (std::size_t i = 0; i < materialCount; ++i)
                                {
                                    const bool selected = selectedMaterialIndex == static_cast<int>(i);
                                    const std::string label = "Material " + std::to_string(i);
                                    if (ImGui::Selectable(label.c_str(), selected))
                                        selectedMaterialIndex = static_cast<int>(i);
                                    if (selected)
                                        ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            if (selectedMaterialIndex >= 0 && static_cast<std::size_t>(selectedMaterialIndex) < materialCount)
                            {
                                Material& material = selectedObject->model->getMaterial(static_cast<std::size_t>(selectedMaterialIndex));
                                inspectorProperty("Base Color");
                                ImGui::ColorEdit4("##BaseColor", &material.baseColorFactor.x,
                                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                                inspectorProperty("Emission");
                                ImGui::ColorEdit3("##Emission", &material.emissiveFactor.x,
                                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
                                inspectorProperty("Strength");
                                ImGui::DragFloat("##EmissiveStrength", &material.emissiveStrength,
                                    0.1f, 0.0f, 10000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                                inspectorProperty("Metallic");
                                ImGui::SliderFloat("##Metallic", &material.metallicFactor, 0.0f, 1.0f, "%.2f");
                                inspectorProperty("Roughness");
                                ImGui::SliderFloat("##Roughness", &material.roughnessFactor, 0.0f, 1.0f, "%.2f");
                                inspectorProperty("Two-sided");
                                ImGui::Checkbox("##DoubleSided", &material.doubleSided);
                                inspectorProperty("Alpha Mode");
                                ImGui::Combo("##AlphaMode", &material.alphaMode, "Opaque\0Mask\0Blend\0");
                                if (material.alphaMode == 1)
                                {
                                    inspectorProperty("Cutoff");
                                    ImGui::SliderFloat("##AlphaCutoff", &material.alphaCutoff, 0.0f, 1.0f, "%.2f");
                                }
                                if (material.alphaMode == 2)
                                {
                                    inspectorProperty("Opacity");
                                    ImGui::SliderFloat("##Opacity", &material.baseColorFactor.a, 0.0f, 1.0f, "%.2f");
                                }
                            }
                            ImGui::EndTable();
                        }
                        if (selectedMaterialIndex < 0 || static_cast<std::size_t>(selectedMaterialIndex) >= materialCount)
                            ImGui::TextWrapped("Click a surface in the viewport or choose a material slot.");
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.15f, 0.17f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.43f, 0.20f, 0.23f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.53f, 0.23f, 0.26f, 1.0f));
                    if (ImGui::Button("Delete Object", ImVec2(-1.0f, 0.0f)))
                    {
                        SceneObject* objectToDelete = selectedObject;
                        selectedObject = nullptr;
                        selectedMaterialIndex = -1;
                        scene.removeObject(objectToDelete);
                    }
                    ImGui::PopStyleColor(3);
                }
                else
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("NO SELECTION");
                    ImGui::TextWrapped("Select an object in the scene or hierarchy to inspect its properties.");
                }
                ImGui::End();

                const ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoDecoration |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoScrollbar;
                ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x,
                    viewport->WorkPos.y + viewport->WorkSize.y - statusHeight));
                ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusHeight));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 5.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::Begin("EditorStatus", nullptr, statusFlags);
                const float fps = ImGui::GetIO().Framerate;
                ImGui::TextDisabled("%.0f FPS  /  %.2f ms", fps, fps > 0.0f ? 1000.0f / fps : 0.0f);
                ImGui::SameLine(0.0f, 24.0f);
                ImGui::TextDisabled("%d x %d", viewportRenderWidth, viewportRenderHeight);
                ImGui::SameLine(0.0f, 24.0f);
                ImGui::TextDisabled(renderer.isTAAEnabled() ? "TAA ON" : "TAA OFF");
                if (viewport->WorkSize.x > 1000.0f)
                {
                    ImGui::SameLine(0.0f, 24.0f);
                    ImGui::TextDisabled("HDR: %s", environmentName.empty() ? "None (File > Import HDR...)" : environmentName.c_str());
                }
                ImGui::End();
                ImGui::PopStyleVar(2);
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (viewportResizePending)
                renderer.resizeSceneFramebuffer(viewportRenderWidth, viewportRenderHeight);

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