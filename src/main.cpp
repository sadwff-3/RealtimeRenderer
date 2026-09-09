#include"Shader.h"
#include"Texture.h"
#include"Camera.h"
#include"Material.h"
#include"Mesh.h"
#include"Model.h"
#include"Light.h"
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
    glViewport(0, 0, 1280, 720);
    
    glfwSetFramebufferSizeCallback(window,framebuffer_size_callback);
    glfwSetCursorPosCallback(window,mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
    
    //shader¥Õ¥¡¥¤¥ë¤òÕi¤ßÞz¤à
    {
        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        Shader shadowShader("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
        Shader shadowDebugShader("shaders/shadow_debug.vert", "shaders/shadow_debug.frag");
        Texture texture("assets/texture.jpg");
        Texture normalTexture("assets/normal.jpg");
        Model testModel("assets/sponza/sponza.obj");


        std::vector<Vertex> cubeVertices =
        {
            // back (-Z)
            {{-0.5f,-0.5f,-0.5f}, {0,0,-1}, {0,0}},
            {{ 0.5f,-0.5f,-0.5f}, {0,0,-1}, {1,0}},
            {{ 0.5f, 0.5f,-0.5f}, {0,0,-1}, {1,1}},
            {{-0.5f, 0.5f,-0.5f}, {0,0,-1}, {0,1}},

            // front (+Z)
            {{-0.5f,-0.5f, 0.5f}, {0,0,1}, {0,0}},
            {{ 0.5f,-0.5f, 0.5f}, {0,0,1}, {1,0}},
            {{ 0.5f, 0.5f, 0.5f}, {0,0,1}, {1,1}},
            {{-0.5f, 0.5f, 0.5f}, {0,0,1}, {0,1}},

            // left (-X)
            {{-0.5f, 0.5f, 0.5f}, {-1,0,0}, {1,0}},
            {{-0.5f, 0.5f,-0.5f}, {-1,0,0}, {1,1}},
            {{-0.5f,-0.5f,-0.5f}, {-1,0,0}, {0,1}},
            {{-0.5f,-0.5f, 0.5f}, {-1,0,0}, {0,0}},

            // right (+X)
            {{ 0.5f, 0.5f, 0.5f}, {1,0,0}, {1,0}},
            {{ 0.5f, 0.5f,-0.5f}, {1,0,0}, {1,1}},
            {{ 0.5f,-0.5f,-0.5f}, {1,0,0}, {0,1}},
            {{ 0.5f,-0.5f, 0.5f}, {1,0,0}, {0,0}},

            // bottom (-Y)
            {{-0.5f,-0.5f,-0.5f}, {0,-1,0}, {0,1}},
            {{ 0.5f,-0.5f,-0.5f}, {0,-1,0}, {1,1}},
            {{ 0.5f,-0.5f, 0.5f}, {0,-1,0}, {1,0}},
            {{-0.5f,-0.5f, 0.5f}, {0,-1,0}, {0,0}},

            // top (+Y)
            {{-0.5f, 0.5f,-0.5f}, {0,1,0}, {0,1}},
            {{ 0.5f, 0.5f,-0.5f}, {0,1,0}, {1,1}},
            {{ 0.5f, 0.5f, 0.5f}, {0,1,0}, {1,0}},
            {{-0.5f, 0.5f, 0.5f}, {0,1,0}, {0,0}}
        };

        std::vector<unsigned int> cubeIndices =
        {
             0,  1,  2,
             2,  3,  0,

             4,  5,  6,
             6,  7,  4,

             8,  9, 10,
            10, 11,  8,

            12, 13, 14,
            14, 15, 12,

            16, 17, 18,
            18, 19, 16,

            20, 21, 22,
            22, 23, 20
        };

        Mesh cubeMesh(cubeVertices, cubeIndices);


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
        shader.setInt("texture1", 0);
        shader.setInt("shadowMap", 2);


        shadowDebugShader.use();
        shadowDebugShader.setInt("depthMap", 0);


        glEnable(GL_DEPTH_TEST);
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

        glm::vec3 cubePositions[] = {
        glm::vec3(0.0f,  0.0f,  0.0f),
        glm::vec3(2.0f,  0.0f, -3.0f),
        glm::vec3(-1.5f,  1.0f, -2.5f),
        glm::vec3(1.0f, -1.0f, -4.0f),
        glm::vec3(-2.0f,  0.5f, -5.0f)
        };

        //Material
        Material woodMaterial(&texture, &normalTexture, 0.2f, 0.3f, 16.0f);
        Material shinyMaterial(&texture, &normalTexture, 0.1f, 1.0f, 128.0f);
        //Lights
        PointLight lights[2];
        DirectionalLight directionalLight;

        glm::vec3 lightPos = -directionalLight.direction * 20.0f;


        directionalLight.direction = glm::vec3(-0.2f, -0.1f, -0.3f);
        directionalLight.color = glm::vec3(1.0f);
        directionalLight.intensity = 0.8f;

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

            glm::mat4 shadowModel = glm::mat4(1.0f);
            shadowModel = glm::scale(shadowModel, glm::vec3(0.01f));

            shadowShader.setMat4("model", shadowModel);
            testModel.draw(shadowShader);
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, 1280, 720);

            glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.use();

            shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::scale(model, glm::vec3(0.01f));

            model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));

            shader.setMat4("model", model);


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
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), 1280.0f / 720.0f, 0.1f, 100.0f);

            shader.setMat4("view", view);
            shader.setMat4("projection", projection);

            testModel.draw(shader);

            for (int i = 0; i < 5; ++i)
            {
                glm::mat4 model = glm::mat4(1.0f);

                model = glm::translate(
                    model,
                    cubePositions[i]
                );

                shader.setMat4("model", model);

                if (i % 2 == 0)
                {
                    woodMaterial.bind(shader);
                }
                else
                {
                    shinyMaterial.bind(shader);
                }

                cubeMesh.draw();
            }


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