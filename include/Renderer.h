#pragma once

#include "Shader.h"
#include "Light.h"
#include <glm/glm.hpp>
#include<array>

class Scene;
class Camera;
class Mesh;

class Renderer
{
public:
    Renderer(int width, int height);
    ~Renderer();

    glm::mat4 renderShadowPass(const Scene& scene, const DirectionalLight& light);

    void renderScenePass(const Scene& scene, Camera& camera, const DirectionalLight& directionalLight, const PointLight* poinLights, int pointLightCount, const glm::mat4& lightSpaceMatrix);

    void beginScenePass();
    void resizeSceneFramebuffer(int width, int height);
    void renderScreenPass(Mesh& quad, int toneMappingMode, int debugView);
    void renderGeometryPass(const Scene& scene, Camera& camera);
    void renderSSAOPass(Mesh& quad, Camera& camera);
    void renderSSAOBlurPass(Mesh& quad);
    void renderDeferredLightingPass(Mesh& quad, Camera& camera, const DirectionalLight& directionalLight, const PointLight* pointLights, int pointLightCount, const glm::mat4& lightSpaceMatrix, int debugView);

    unsigned int getShadowMap() const;
    unsigned int getSceneColorTexture() const;

private:
    Shader shadowShader;
    Shader sceneShader;
    Shader screenShader;
    Shader geometryShader;
    Shader ssaoShader;
    Shader ssaoBlurShader;
    Shader deferredShader;

    unsigned int depthMapFBO = 0;
    unsigned int depthMap = 0;

    static constexpr unsigned int SHADOW_WIDTH = 2048;
    static constexpr unsigned int SHADOW_HEIGHT = 2048;

    unsigned int sceneFBO = 0;
    unsigned int sceneColorTexture = 0;
    unsigned int sceneRBO = 0;
    unsigned int gBuffer = 0;
    unsigned int gNormal = 0;
    unsigned int gAlbedo = 0;
    unsigned int gMaterial = 0;
    unsigned int gDepth = 0;
    unsigned int ssaoFBO = 0;
    unsigned int ssaoTexture = 0;
    unsigned int ssaoBlurFBO = 0;
    unsigned int ssaoBlurTexture = 0;
    std::array<glm::vec3,32> ssaoKernel{};
    unsigned int ssaoNoiseTexture = 0;

    int framebufferWidth = 0;
    int framebufferHeight = 0;

    void initShadowMap();
    void initSceneFramebuffer();
    void initGBuffer();
    void initSSAOFramebuffer();
    void initSSAOKernel();
    void initSSAONoiseTexture();
    void initSSAOBlur();
  
};