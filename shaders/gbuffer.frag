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

void main()
{
    vec3 albedo = texture(albedoMap, TexCoord).rgb;

    vec3 tangentNormal = texture(normalMap, TexCoord).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;
    vec3 normal = normalize(TBN * tangentNormal);

    float metallic = texture(metallicMap, TexCoord).b;
    float roughness = texture(roughnessMap, TexCoord).g;
    float ao = texture(aoMap, TexCoord).r;

    gNormal = normal;
    gAlbedo = vec4(albedo, 1.0);
    gMaterial = vec4(metallic, roughness, ao, 1.0);
}