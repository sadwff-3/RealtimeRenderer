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

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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
        Shader shader("shaders/pbr.vert", "shaders/pbr.frag");
        Shader screenShader("shaders/screen.vert", "shaders/screen.frag");
        Shader shadowShader("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
        Shader shadowDebugShader("shaders/shadow_debug.vert", "shaders/shadow_debug.frag");
       
        Model testModel("assets/sponza_pbr/Sponza.gltf");
        Model helmetModel("assets/flight_helmet/FlightHelmet.gltf");
        
        Scene scene;
        SceneObject& testObject = scene.addObject("Sponza", &testModel);

        SceneObject& helmet1 = scene.addObject("Helmet1", &helmetModel);
        SceneObject& helmet2 = scene.addObject("Helmet2", &helmetModel);

        helmet1.transform.position = glm::vec3(-1.5f, 0.0f, 0.0f);
        helmet2.transform.position = glm::vec3(1.5f, 0.0f, 0.0f);




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


        shader.use();
        
        shader.setInt("shadowMap", 2);


        shadowDebugShader.use();
        shadowDebugShader.setInt("depthMap", 0);
        screenShader.use();
        screenShader.setInt("screenTexture", 0);
        screenShader.setFloat("exposure", 1.0f);


        glEnable(GL_DEPTH_TEST);

        unsigned int sceneFBO;
        glGenFramebuffers(1, &sceneFBO);
        glBindFramebuffer(GL_FRAMEBUFFER , sceneFBO);

        //color attchment
        unsigned int sceneColorTexture;
        glGenTextures(1, &sceneColorTexture);
        glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);

        //Depth
        unsigned int sceneRBO;
        glGenRenderbuffers(1, &sceneRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebufferWidth, framebufferHeight);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneRBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "Scene framebuffer is not complete!\n";
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        //shadow map create
        const unsigned int SHADOW_WIDTH = 2048;
        const unsigned int SHADOW_HEIGHT = 2048;
       
        unsigned int depthMapFBO;
        glGenFramebuffers(1, &depthMapFBO);

        unsigned int depthMap; 
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;
        int toneMappingMode = 1;
        
        //Lights
        PointLight lights[2];
        DirectionalLight directionalLight;

       


        directionalLight.direction = glm::vec3(-0.2f, -0.1f, -0.3f);
        directionalLight.color = glm::vec3(1.0f);
        directionalLight.intensity = 0.8f;

        glm::vec3 lightPos = -directionalLight.direction * 20.0f;

        lights[0].position = glm::vec3(1.2f, 1.0f, 2.0f);
        lights[0].color = glm::vec3(1.0f, 1.0f, 1.0f);
        lights[0].intensity = 1.0f;

        lights[0].constant = 1.0f;
        lights[0].linear = 0.09f;
        lights[0].quadratic = 0.032f;

        lights[1].position = glm::vec3(-2.0f, 1.0f, 1.0f);
        lights[1].color = glm::vec3(1.0f, 0.2f, 0.2f);
        lights[1].intensity = 0.7f;

        lights[1].constant = 1.0f;
        lights[1].linear = 0.09f;
        lights[1].quadratic = 0.032f;

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

            if (framebufferWidth == 0 || framebufferHeight == 0)
            {
                glfwPollEvents();
                continue;
            }

            if (framebufferResized)
            {
                glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);

                glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebufferWidth, framebufferHeight);

                framebufferResized = false;
            }


     //Shadow Pass

            //shadow mapping
            glm::mat4 lightProjection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 50.0f);
            glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightSpaceMatrix = lightProjection * lightView;
            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
            
            
            shadowShader.use();
            shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
            
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, depthMap);

            for (const auto& object : scene.getObjects())
            {
                glm::mat4 model = object->transform.getMatrix();
                shadowShader.setMat4("model", model);
                object->model->draw(shadowShader);
            }
            
     //Scene Pass

            glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
            glViewport(0, 0, framebufferWidth, framebufferHeight);

            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.use();

            shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);


            //DirectionalLightPos
            shader.setVec3("directionalLight.direction", directionalLight.direction);
            shader.setVec3("directionalLight.color", directionalLight.color);
            shader.setFloat("directionalLight.intensity", directionalLight.intensity);

            //PointlightPos
            shader.setInt("pointLightCount", 2);
            for (int i = 0; i < 2; ++i)
            {
                std::string base = "pointLights[" + std::to_string(i) + "]";

                shader.setVec3((base + ".position").c_str(), lights[i].position);
                shader.setVec3((base + ".color").c_str(), lights[i].color);
                shader.setFloat((base + ".intensity").c_str(), lights[i].intensity);

                shader.setFloat(base + ".constant", lights[i].constant);
                shader.setFloat(base + ".linear", lights[i].linear);
                shader.setFloat(base + ".quadratic", lights[i].quadratic);
            }

            //viewerPos
            shader.setVec3("viewPos", camera.Position);

            //view
            glm::mat4 view = camera.getViewMatrix();

            //projection
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight), 0.1f, 100.0f);

            shader.setMat4("view", view);
            shader.setMat4("projection", projection);

            for (const auto& object : scene.getObjects())
            {
                glm::mat4 model = object->transform.getMatrix();
                shader.setMat4("model", model);
                object->model->draw(shader);
            }

      //Screen Pass

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, framebufferWidth, framebufferHeight);

            glDisable(GL_DEPTH_TEST);

            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            screenShader.use();
            screenShader.setInt("toneMappingMode", toneMappingMode);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sceneColorTexture);

            debugQuad.draw();

            glEnable(GL_DEPTH_TEST);

            if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
            {
                glDisable(GL_DEPTH_TEST);

                shadowDebugShader.use();

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, depthMap);

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