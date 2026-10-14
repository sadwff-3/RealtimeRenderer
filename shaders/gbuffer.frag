#version 330 core

layout (location = 0) out vec3 gNormal;
layout (location = 1) out vec4 gAlbedo;
layout (location = 2) out vec4 gMaterial;
layout (location = 3) out uint gObjectID;
layout (location = 4) out uint gMaterialID;
layout (location = 5) out vec2 gVelocity;
layout (location = 6) out vec3 gEmission;

in vec2 TexCoord;
in mat3 TBN;
in vec4 CurrentClip;
in vec4 PreviousClip;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D emissiveMap;
uniform bool hasEmissiveMap;
uniform vec3 emissiveFactor;
uniform float emissiveStrength;
uniform vec4 baseColorFactor;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform float normalYSign;
uniform int alphaMode;
uniform float alphaCutoff;
uniform bool doubleSided;
uniform int objectID;
uniform int materialID;


vec3 fallbackTangent(vec3 N)
{
    vec3 helper = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);

    return normalize(cross(helper, N));
}


void main()
{
    vec4 baseColor = texture(albedoMap, TexCoord) * baseColorFactor;
    if (alphaMode == 1 && baseColor.a < alphaCutoff) discard;

    vec3 N = normalize(TBN[2]);
    vec3 T = TBN[0] - N * dot(N, TBN[0]);

    if (dot(T, T) > 1e-10)
        T = normalize(T);
    else
        T = fallbackTangent(N);

    float handedness = dot(cross(N, T), TBN[1]) < 0.0 ? -1.0 : 1.0;

    vec3 B = handedness * normalize(cross(N, T));

    vec2 normalXY = texture(normalMap, TexCoord).rg * 2.0 - 1.0;

    normalXY.y *= normalYSign;

    float normalZ = sqrt(max(1.0 - dot(normalXY, normalXY), 0.0));

    vec3 tangentNormal = normalize(vec3(normalXY, normalZ));

    vec3 normal = normalize(mat3(T, B, N) * tangentNormal);

    if (doubleSided && !gl_FrontFacing)
        normal = -normal;

    float metallic = texture(metallicMap, TexCoord).b * metallicFactor;

    float roughness = texture(roughnessMap, TexCoord).g * roughnessFactor;

    float ao = texture(aoMap, TexCoord).r;

    vec3 emissiveTexel = hasEmissiveMap ? texture(emissiveMap, TexCoord).rgb : vec3(1.0);
    gEmission = emissiveTexel * emissiveFactor * emissiveStrength;

    gNormal = normal;
    gAlbedo = vec4(baseColor.rgb, 1.0);
    gMaterial = vec4(metallic, roughness, ao, 1.0);
    gObjectID = uint(objectID);
    gMaterialID = uint(materialID);

    if(CurrentClip.w <= 0.0 || PreviousClip.w <= 0.0)
    {
        gVelocity = vec2(0.0);
    }
    else
    {
        vec2 currentUV = (CurrentClip.xy / CurrentClip.w) * 0.5 + 0.5;
        vec2 previousUV = (PreviousClip.xy / PreviousClip.w) * 0.5 + 0.5;

        gVelocity = currentUV - previousUV;
    }
}