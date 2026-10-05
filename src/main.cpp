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

//Camera
Camera camera;
float lastX = 640.0f;
float lastY = 360.0f;
bool firstMouse = true;
bool viewportHovered = false;
bool cameraMouseLocked = false;

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
    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    camera.processMouseScroll(static_cast<float>(yoffset));
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

        Renderer renderer(framebufferWidth, framebufferHeight);
        EnvironmentMap environmentMap;
        Texture testHDR("assets/apartment/cayley_interior_4k.hdr");

        if (testHDR.valid())
        {
            environmentMap.generate(testHDR);
            environmentMap.generateIrradiance();
            environmentMap.generatePrefilter();
            environmentMap.generateBRDFLUT();
        }

        Model* apartment =
            assets.loadModel("assets/apartment/scene.gltf", true);

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
        sun.transform.rotation = glm::vec3(-29.0f, 59.0f, 0.0f);

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
        spot.transform.rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
      
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

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(window, true);
            }

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            {
                camera.processKeyboard(CameraMovement::Forward, deltaTime);
            }

            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            {
                camera.processKeyboard(CameraMovement::Backward, deltaTime);
            }

            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            {
                camera.processKeyboard(CameraMovement::Left, deltaTime);
            }

            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            {
                camera.processKeyboard(CameraMovement::Right, deltaTime);
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

            if (framebufferWidth == 0 || framebufferHeight == 0)
            {
                glfwPollEvents();
                continue;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
         //DirectionalShadowPass
            renderer.renderDirectionalShadowPass(scene);

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

            ImGui::Begin("Hierarchy");
            for (const auto& object : scene.getObjects())
            {
                bool selected = (selectedObject == object.get());

                if (ImGui::Selectable(object->name.c_str(), selected))
                {
                    selectedObject = object.get();
                }
            }
            ImGui::End();

            ImGui::Begin("Viewport");
            viewportHovered = ImGui::IsWindowHovered();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            
            int newViewportWidth = static_cast<int>(viewportSize.x);
            int newViewportHeight = static_cast<int>(viewportSize.y);
            if (newViewportWidth > 0 && newViewportHeight > 0 && (newViewportWidth != viewportRenderWidth || newViewportHeight != viewportRenderHeight))
            {
                viewportRenderWidth = newViewportWidth;viewportRenderHeight = newViewportHeight;

                renderer.resizeSceneFramebuffer(viewportRenderWidth, viewportRenderHeight);
            }

            ImGui::Image((ImTextureID)(intptr_t)renderer.getFinalColorTexture(), viewportSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::End();

            ImGui::Begin("Inspector");
            if (selectedObject != nullptr)
            {
                ImGui::TextUnformatted(selectedObject->name.c_str());
                ImGui::Separator();
                ImGui::TextUnformatted("Transform");
                ImGui::DragFloat3("Position", &selectedObject->transform.position.x, 0.05f);
                ImGui::DragFloat3("Rotation", &selectedObject->transform.rotation.x, 0.5f);
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