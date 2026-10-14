#pragma once

#include "Texture.h"
#include "Shader.h"
#include <glm/glm.hpp>

class Material
{
public:
    Texture* albedo;
    Texture* normalMap;
    Texture* metallicMap;
    Texture* roughnessMap;
    Texture* aoMap;

    Texture* emissiveMap = nullptr;
    glm::vec3 emissiveFactor{0.0f};
    float emissiveStrength = 1.0f;

    glm::vec4 baseColorFactor{1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalYSign = 1.0f;
    int alphaMode = 0;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;

    float ambientStrength;
    float specularStrength;
    float shininess;

    Material(
        Texture* albedoTexture,
        Texture* normalTexture,
        Texture* metallicMap,
        Texture* roughnessMap,
        Texture* aoMap
    );

    void bind(Shader& shader) const;
};