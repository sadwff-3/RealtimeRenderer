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

    glm::mat4 renderShadowPass(const Scene& scene);
    void renderDirectionalShadowPass(const Scene& scene);
    void renderSpotShadowPass(const Scene&scene);
    void renderPointShadowPass(const Scene& scene);

    void renderScenePass(const Scene& scene, Camera& camera, const DirectionalLight& directionalLight, const PointLight* poinLights, int pointLightCount, const glm::mat4& lightSpaceMatrix);

    void beginScenePass();
    void resizeSceneFramebuffer(int width, int height);
    void renderScreenPass(Mesh& quad, int toneMappingMode, int debugView);
    void renderGeometryPass(const Scene& scene, Camera& camera);
    void renderSSAOPass(Mesh& quad, Camera& camera);
    void renderSSAOBlurPass(Mesh& quad);
    void renderDeferredLightingPass(Mesh& quad, Camera& camera, const Scene& scene, int debugView);

    unsigned int getShadowMap() const;
    unsigned int getSceneColorTexture() const;

private:
    Shader shadowShader;
    Shader pointShadowShader;
    Shader sceneShader;
    Shader screenShader;
    Shader geometryShader;
    Shader ssaoShader;
    Shader ssaoBlurShader;
    Shader deferredShader;

    unsigned int depthMapFBO = 0;
    unsigned int depthMap = 0;

    static constexpr int DIRECTIONAL_SHADOW_SIZE = 2048;
    static constexpr int MAX_DIRECTIONAL_SHADOWS = 4;
    static constexpr int POINT_SHADOW_SIZE = 512;
    static constexpr int MAX_POINT_SHADOWS = 4;
    static constexpr int SPOT_SHADOW_SIZE = 1024;
    static constexpr int MAX_SPOT_SHADOWS = 10;

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
    unsigned int directionalShadowFBO = 0;
    unsigned int directionalShadowArray = 0;
    unsigned int spotShadowFBO = 0;
    unsigned int spotShadowArray = 0;
    unsigned int pointShadowFBO = 0;
    unsigned int pointShadowArray = 0;

    std::array<glm::mat4, MAX_SPOT_SHADOWS> spotLightSpaceMatrices{};
    std::array<glm::mat4, MAX_DIRECTIONAL_SHADOWS> directionalLightSpaceMatrices{};
    
    int directionalShadowCount = 0;
    int spotShadowCount = 0;
    int pointShadowCount = 0;

    int framebufferWidth = 0;
    int framebufferHeight = 0;

  
    void initSceneFramebuffer();
    void initGBuffer();
    void initSSAOFramebuffer();
    void initSSAOKernel();
    void initSSAONoiseTexture();
    void initSSAOBlur();
    void initDirectionalShadowMaps();
    void initSpotShadowMaps();
    void initPointShadowMaps();
  
};