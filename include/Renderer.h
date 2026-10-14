#pragma once

#include"Shader.h"
#include"Light.h"
#include<glm/glm.hpp>
#include<array>
#include<vector>
#include<unordered_map>

class Scene;
class Camera;
class Mesh;
class SceneObject;

class Renderer
{
public:
    Renderer(int width, int height);
    ~Renderer();

    glm::mat4 renderShadowPass(const Scene& scene);
    void renderDirectionalShadowPass(const Scene& scene, const Camera& camera);
    void renderSpotShadowPass(const Scene&scene);
    void renderPointShadowPass(const Scene& scene);

    void renderScenePass(const Scene& scene, Camera& camera, const DirectionalLight& directionalLight, const PointLight* poinLights, int pointLightCount, const glm::mat4& lightSpaceMatrix);

    void beginScenePass();
    void resizeSceneFramebuffer(int width, int height);
    void renderScreenPass(Mesh& quad, int toneMappingMode, int debugView);
    void renderGeometryPass(const Scene& scene, Camera& camera);
    void renderSSAOPass(Mesh& quad, Camera& camera);
    void renderSSAOBlurPass(Mesh& quad);
    void renderDeferredLightingPass(Mesh& quad, Camera& camera, const Scene& scene, int debugView, unsigned int environmentCubemap, unsigned int irradianceMap, unsigned int prefilterMap, unsigned int brdfLUT);
    void beginTAAFrame(const Camera& camera);
    void renderTAAPass(Mesh& quad, int debugView);
    void renderBloomPass(Mesh& quad, int debugView);
    void setTAAEnabled(bool enabled);
    bool isTAAEnabled() const { return taaEnabled; };
    void destroyBloomFramebuffer();

    struct BloomSettings
    {
        bool enabled = true;
        float intensity = 0.08f;
        float threshold = 1.0f;
        float softKnee = 0.5f;
    };

    BloomSettings bloom;
    int pointShadowBudget = 1; // Maximum shadowed point lights; other point lights still illuminate.

    unsigned int getShadowMap() const;
    unsigned int getSceneColorTexture() const;
    unsigned int getFinalColorTexture() const;
    unsigned int readObjectID(int x, int y) const;
    unsigned int readMaterialID(int x, int y) const;
    std::vector<glm::vec4> getFrustumCornersWorldSpace(const Camera& camera, float aspectRatio, float nearPlane, float farPlane) const;
    glm::mat4 calculateCascadeLightMatrix(const glm::vec3& lightDirection, const Camera& camera, float aspectRatio, float nearPlane, float farPlane) const;

private:
    Shader shadowShader;
    Shader pointShadowShader;
    Shader sceneShader;
    Shader screenShader;
    Shader geometryShader;
    Shader ssaoShader;
    Shader ssaoBlurShader;
    Shader deferredShader;
    Shader taaShader;
    Shader bloomDownsampleShader;
    Shader bloomUpsampleShader;

    unsigned int depthMapFBO = 0;
    unsigned int depthMap = 0;

    static constexpr int DIRECTIONAL_SHADOW_SIZE = 2048;
    static constexpr float DIRECTIONAL_SHADOW_DISTANCE = 120.0f;
    static constexpr float DIRECTIONAL_SHADOW_HALF_EXTENT = 60.0f;
    static constexpr float DIRECTIONAL_SHADOW_DEPTH = 240.0f;
    static constexpr int MAX_DIRECTIONAL_SHADOWS = 4;
    static constexpr int POINT_SHADOW_SIZE = 512;
    static constexpr int MAX_POINT_SHADOWS = 4;
    static constexpr int SPOT_SHADOW_SIZE = 1024;
    static constexpr int MAX_SPOT_SHADOWS = 10;
    static constexpr int CASCADE_COUNT = 4;

    unsigned int sceneFBO = 0;
    unsigned int sceneColorTexture = 0;
    unsigned int sceneRBO = 0;
    unsigned int gBuffer = 0;
    unsigned int gNormal = 0;
    unsigned int gAlbedo = 0;
    unsigned int gMaterial = 0;
    unsigned int gObjectID = 0;
    unsigned int gMaterialID = 0;
    unsigned int gDepth = 0;
    unsigned int gVelocity = 0;
    unsigned int gEmission = 0;

    struct BloomMip
    {
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
    };

    std::vector<BloomMip> bloomMips;
    unsigned int bloomFBO = 0;
    bool bloomReady = false;

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
    unsigned int finalFBO = 0;
    unsigned int finalColorTexture = 0;
    bool taaEnabled = false;
    unsigned int taaFrameIndex = 0;
    

    std::array<glm::mat4, MAX_SPOT_SHADOWS> spotLightSpaceMatrices{};
    std::array<glm::mat4, MAX_DIRECTIONAL_SHADOWS> directionalLightSpaceMatrices{};
    std::array<glm::mat4, MAX_DIRECTIONAL_SHADOWS * CASCADE_COUNT> directionalCascadeMatrices{};
    std::array<float, CASCADE_COUNT> cascadeSplits{};
    std::array<float, CASCADE_COUNT> calculateCascadeSplits(float nearPlane, float farPlane) const;
    std::array<unsigned int, 2> taaHistoryFBO{};
    std::array<unsigned int, 2> taaHistoryColor{};

    int directionalShadowCount = 0;
    int spotShadowCount = 0;
    int pointShadowCount = 0;

    int framebufferWidth = 0;
    int framebufferHeight = 0;

    int taaHistoryReadIndex = 0;
    bool taaHistoryValid = false;
    std::size_t taaSceneSignature = 0;
    std::size_t taaAppearanceSignature = 0;
    bool hasTAASceneSignature = false;
    int taaStableFrames = 0;

    glm::vec2 taaJitterPixels{0.0f};
    glm::vec2 previousTAAJitterPixels{0.0f};
    glm::mat4 frameView{1.0f};
    glm::mat4 frameProjection{1.0f};
    glm::mat4 previousViewProjection{1.0f};
    bool hasPreviousTAAFrame = false;
    std::unordered_map<const SceneObject* ,glm::mat4>previousModelMatrices;
  
    void initSceneFramebuffer();
    void initGBuffer();
    void initSSAOFramebuffer();
    void initSSAOKernel();
    void initSSAONoiseTexture();
    void initSSAOBlur();
    void initDirectionalShadowMaps();
    void initSpotShadowMaps();
    void initPointShadowMaps();
    void initFinalFramebuffer();
    void initTAAFramebuffers();
    void initBloomFramebuffer();
    unsigned int getPostTAAColorTexture(int debugView) const;
};
