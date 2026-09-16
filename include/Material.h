#pragma once

#include "Texture.h"
#include "Shader.h"

class Material
{
public:
    Texture* albedo;
    Texture* normalMap;
    Texture* metallicMap;
    Texture* roughnessMap;
    Texture* aoMap;

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