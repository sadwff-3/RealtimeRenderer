#version 450 core

in vec2 TexCoord;

layout(location = 0) out uint pickedObjectID;
layout(location = 1) out uint pickedMaterialID;

uniform sampler2D albedoMap;
uniform vec4 baseColorFactor;

uniform int objectID;
uniform int materialID;

void main()
{
    float alpha = texture(albedoMap, TexCoord).a * baseColorFactor.a;


    if (alpha <= 0.0)
        discard;

    pickedObjectID = uint(objectID);
    pickedMaterialID = uint(materialID);
}