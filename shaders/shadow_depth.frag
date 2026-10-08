#version 330 core

in vec2 TexCoord;

uniform sampler2D albedoMap;
uniform vec4 baseColorFactor;
uniform int alphaMode;
uniform float alphaCutoff;

void main()
{
    if (alphaMode == 1 && texture(albedoMap, TexCoord).a * baseColorFactor.a < alphaCutoff) discard;
}