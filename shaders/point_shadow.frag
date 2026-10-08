#version 330 core

in vec3 FragPos;

uniform vec3 lightPos;
uniform float farPlane;

in vec2 TexCoord;

uniform sampler2D albedoMap;
uniform vec4 baseColorFactor;
uniform int alphaMode;
uniform float alphaCutoff;

void main()
{
    if (alphaMode == 1 && texture(albedoMap, TexCoord).a * baseColorFactor.a < alphaCutoff) discard;
    float lightDistance = length(FragPos - lightPos);

    lightDistance /= farPlane;

    gl_FragDepth = lightDistance;
}