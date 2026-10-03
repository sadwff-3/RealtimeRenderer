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

//Camera
Camera camera;
float lastX = 640.0f;
float lastY = 360.0f;
bool firstMouse = true;

//framebuffer size
int framebufferWidth = 1280;
int framebufferHeight = 720;
bool framebufferResized = false;

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
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

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    framebufferWidth = width;
    framebufferHeight = height;
    framebufferResized = true;
    glViewport(0, 0, width, height);
}

void scroll_callback(GLFWwindow* window,double xoffset,double yoffset)
{
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
    
    glfwSetFramebufferSizeCallback(window,framebuffer_size_callback);
    glfwSetCursorPosCallback(window,mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
    
    //shader¥Õ¥¡¥¤¥ë¤òÕi¤ßÞz¤à
    {
        
        Shader shadowDebugShader("shaders/shadow_debug.vert", "shaders/shadow_debug.frag");
        
        AssetManager assets;
        Scene scene;
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

        Model* livingRoom = assets.loadModel("assets/Bistro/BistroExterior.gltf");

        if (livingRoom)
        {
            scene.addObject("LivingRoom", livingRoom);
        }
        else
        {
            std::cerr << "Failed to load Sponza\n";
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

            if (framebufferResized)
            {
                renderer.resizeSceneFramebuffer(framebufferWidth, framebufferHeight);
                framebufferResized = false;
            }
    
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


            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}