#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMaterial;
uniform sampler2D gDepth;
uniform sampler2D shadowMap;
uniform sampler2D ssaoMap;

uniform mat4 inverseViewProjection;
uniform mat4 lightSpaceMatrix;
uniform vec3 viewPos;
uniform int debugView;

struct DirectionalLight
{
    vec3 direction;
    vec3 color;
    float intensity;
};

struct PointLight
{
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

#define MAX_POINT_LIGHTS 4

uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int pointLightCount;

uniform DirectionalLight directionalLight;

const float PI = 3.14159265359;

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (any(lessThan(projCoords, vec3(0.0))) || any(greaterThan(projCoords, vec3(1.0))))
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

vec3 reconstructWorldPosition(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = inverseViewProjection * clipPos;
    return worldPos.xyz / worldPos.w;
}

void main()
{
    float depth = texture(gDepth, TexCoord).r;

    if(debugView == 6)
    {
        const float nearPlane = 0.1;
        const float farPlane = 100.0;

        if(depth >= 1.0)
        {
            FragColor = vec4(vec3(1.0), 1.0);
            return;
        }

        float z = depth * 2.0 - 1.0;
        float linearDepth = (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
        float visualizationFar = 20.0;
        float gray = clamp((linearDepth - nearPlane) / (visualizationFar - nearPlane), 0.0, 1.0);

        FragColor = vec4(vec3(gray), 1.0);
        return;
    }

    if (debugView == 7)
    {
        float ssao = texture(ssaoMap, TexCoord).r;
        FragColor = vec4(vec3(ssao), 1.0);
        return;
    } 

    if(depth >= 1.0)
    {
        FragColor = vec4(0.1, 0.2, 0.3, 1.0);
        return;
    }

    vec3 fragPos = reconstructWorldPosition(TexCoord, depth);

    vec3 normal = normalize(texture(gNormal, TexCoord).rgb);
    vec3 albedo = texture(gAlbedo, TexCoord).rgb;

    vec3 material = texture(gMaterial, TexCoord).rgb;
    float metallic = material.r;
    float roughness = material.g;
    float ao = material.b;

    if(debugView == 1)
    {
        FragColor = vec4(albedo, 1.0);
        return;
    }

    if(debugView == 2)
    {
        FragColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }

    if(debugView == 3)
    {
        FragColor = vec4(vec3(metallic), 1.0);
        return;
    }

    if(debugView == 4)
    {
        FragColor = vec4(vec3(roughness), 1.0);
        return;
    }

    if(debugView == 5)
    {
        FragColor = vec4(vec3(ao), 1.0);
        return;
    }

    vec3 N = normal;
    vec3 V = normalize(viewPos - fragPos);
    vec3 L = normalize(-directionalLight.direction);
    vec3 H = normalize(V + L);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H,V), 0.0), F0);
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    vec3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 diffuse = kD * albedo / PI;
    
    vec3 radiance = directionalLight.color * directionalLight.intensity;
    vec3 directLighting = (diffuse + specular) * radiance * NdotL;

    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
    float shadow = calculateShadow(fragPosLightSpace, N, L);

    float ssao = texture(ssaoMap, TexCoord).r;
    vec3 ambient = vec3(0.12) * albedo * ao * ssao;
    vec3 lighting = ambient + (1.0 - shadow) * directLighting;

    for (int i = 0; i < min(pointLightCount, MAX_POINT_LIGHTS); ++i)
    {
        vec3 lightVector = pointLights[i].position - fragPos;
        float distance = length(lightVector);
        vec3 L = normalize(lightVector);
        vec3 H = normalize(V + L);

        float attenuation = 1.0 / (pointLights[i].constant + pointLights[i].linear * distance + pointLights[i].quadratic * distance * distance);

        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float D = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);

        vec3 specular = (D * F * G) / (4.0 * NdotV * NdotL + 0.0001);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        vec3 diffuse = kD * albedo / PI;

        vec3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;

        lighting += (diffuse + specular) * radiance * NdotL;
    }

    FragColor = vec4(lighting, 1.0);
}