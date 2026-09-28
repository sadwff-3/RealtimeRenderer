#version 330 core

layout (location = 0) out vec3 gNormal;
layout (location = 1) out vec4 gAlbedo;
layout (location = 2) out vec4 gMaterial;

in vec2 TexCoord;
in mat3 TBN;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform vec4 baseColorFactor;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform float normalYSign;
uniform int alphaMode;
uniform float alphaCutoff;

void main()
{
    vec4 baseColor = texture(albedoMap, TexCoord) * baseColorFactor;
    if (alphaMode == 1 && baseColor.a < alphaCutoff) discard;
    vec3 albedo = baseColor.rgb;

    vec3 tangentNormal = texture(normalMap, TexCoord).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;
    tangentNormal.y *= normalYSign;
    vec3 normal = normalize(TBN * tangentNormal);

    float metallic = texture(metallicMap, TexCoord).b * metallicFactor;
    float roughness = texture(roughnessMap, TexCoord).g * roughnessFactor;
    float ao = texture(aoMap, TexCoord).r;

    gNormal = normal;
    gAlbedo = vec4(albedo, 1.0);
    gMaterial = vec4(metallic, roughness, ao, 1.0);
}