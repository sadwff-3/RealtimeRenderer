#pragma once

#include"Shader.h"

#include <glad/gl.h>
#include <string>

class Texture;

class EnvironmentMap
{
public:
    EnvironmentMap();
    ~EnvironmentMap();

    bool loadHDR(const std::string& path);
    void generate(Texture& hdrTexture);
    void generateIrradiance();
    void generatePrefilter();
    void generateBRDFLUT();

    GLuint getCubemap() const
    {
        return cubemap;
    }

    GLuint getIrradianceMap() const
    {
        return irradianceMap;
    }

    GLuint getPrefilterMap() const
    {
        return prefilterMap;
    }

    GLuint getBRDFLUT() const
    {
        return brdfLUT;
    }

private:
    GLuint cubemap = 0;
    GLuint irradianceMap = 0;
    GLuint prefilterMap = 0;

    GLuint captureFBO = 0;
    GLuint captureRBO = 0;

    GLuint cubeVAO = 0;
    GLuint cubeVBO = 0;

    GLuint brdfLUT = 0;
    GLuint brdfVAO = 0;

    Shader equirectangularShader;
    Shader irradianceShader;
    Shader prefilterShader;
    Shader brdfShader;

    static constexpr int PREFILTER_SIZE = 128;
    static constexpr int PREFILTER_MIP_LEVELS = 5;
    static constexpr int BRDF_LUT_SIZE = 512;

    void initCube();
    void renderCube() const;
};
