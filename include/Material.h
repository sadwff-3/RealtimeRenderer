#pragma once

#include "Texture.h"
#include "Shader.h"

class Material
{
public:
    Texture* albedo;
    Texture* normalMap;

    float ambientStrength;
    float specularStrength;
    float shininess;

    Material(
        Texture* albedoTexture,
        Texture* normalTexture,
        float ambient = 0.2f,
        float specular = 0.3f,
        float shine = 16.0f
    );

    void bind(Shader& shader) const;
};