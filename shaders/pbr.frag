#version 330 core

in vec4 FragPosLightSpace;
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in mat3 TBN;

out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D normalMap;
uniform sampler2D shadowMap;

struct PointLight
{
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

struct DirectionalLight
{
    vec3 direction;
    vec3 color;
    float intensity;
};

#define MAX_POINT_LIGHTS 4

uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int pointLightCount;
uniform DirectionalLight directionalLight;

uniform vec3 viewPos;

//  ambient 和 point light 还在使用
uniform float ambientStrength;


// PBR 参数
uniform float metallic;
uniform float roughness;
uniform float ao;

const float PI = 3.14159265359;

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
    {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    float shadow = 0.0;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;

            if (currentDepth - bias > closestDepth)
            {
                shadow += 1.0;
            }
        }
    }

    shadow /= 9.0;

    return shadow;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float numerator = a2;

    float denominator = NdotH2 * (a2 - 1.0) + 1.0;
    denominator = PI * denominator * denominator;

    return numerator / denominator;
}

float geometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float numerator = NdotX;
    float denominator = NdotX * (1.0 - k) + k;

    return numerator / denominator;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggxView = geometrySchlickGGX(NdotV, roughness);
    float ggxLight = geometrySchlickGGX(NdotL, roughness);

    return ggxView * ggxLight;
}

void main()
{
    vec3 textureColor = texture(texture1, TexCoord).rgb;
    vec3 albedo = textureColor;

    vec3 tangentNormal = texture(normalMap, TexCoord).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;

    vec3 norm = normalize(TBN * tangentNormal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // PBR 基础反射率
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Ambient 暂时保留旧版本
    vec3 ambient = ambientStrength * textureColor;
    vec3 lighting = ambient;

    // =========================================================
    // Directional Light - PBR
    // =========================================================

    vec3 dirLightDir = normalize(-directionalLight.direction);
    vec3 dirHalfwayDir = normalize(dirLightDir + viewDir);

    vec3 F = fresnelSchlick(max(dot(dirHalfwayDir, viewDir), 0.0), F0);
    float D = distributionGGX(norm, dirHalfwayDir, roughness);
    float G = geometrySmith(norm, viewDir, dirLightDir, roughness);

    float NdotV = max(dot(norm, viewDir), 0.0);
    float NdotL = max(dot(norm, dirLightDir), 0.0);

    vec3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 pbrSpecular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 pbrDiffuse = kD * albedo / PI;

    vec3 radiance = directionalLight.color * directionalLight.intensity;

    vec3 dirLighting = (pbrDiffuse + pbrSpecular) * radiance * NdotL;

    float shadow = calculateShadow(FragPosLightSpace, normalize(Normal), dirLightDir);

    lighting += (1.0 - shadow) * dirLighting;

    // =========================================================
    // Point Lights - 暂时仍然使用 Blinn-Phong
    // =========================================================

    for (int i = 0; i < pointLightCount; ++i)
{
    vec3 lightVector = pointLights[i].position - FragPos;
    float distance = length(lightVector);
    vec3 lightDir = normalize(lightVector);

    float attenuation = 1.0 / (
        pointLights[i].constant
        + pointLights[i].linear * distance
        + pointLights[i].quadratic * distance * distance
    );

    vec3 halfwayDir = normalize(lightDir + viewDir);

    vec3 F = fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0);
    float D = distributionGGX(norm, halfwayDir, roughness);
    float G = geometrySmith(norm, viewDir, lightDir, roughness);

    float NdotV = max(dot(norm, viewDir), 0.0);
    float NdotL = max(dot(norm, lightDir), 0.0);

    vec3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 pbrSpecular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 pbrDiffuse = kD * albedo / PI;

    vec3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;

    lighting += (pbrDiffuse + pbrSpecular) * radiance * NdotL;
}

    FragColor = vec4(lighting, 1.0);
}