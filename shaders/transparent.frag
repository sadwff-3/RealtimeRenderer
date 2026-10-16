#version 450 core

in vec2 TexCoord;
in vec3 FragPos;
in mat3 TBN;

layout(location = 0) out vec4 FragColor;


uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D emissiveMap;

uniform vec4 baseColorFactor;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform float normalYSign;
uniform bool doubleSided;

uniform bool hasEmissiveMap;
uniform vec3 emissiveFactor;
uniform float emissiveStrength;


uniform vec3 viewPos;


uniform sampler2DArray directionalShadowMaps;
uniform sampler2DArray spotShadowMaps;
uniform samplerCubeArray pointShadowMaps;

uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

struct DirectionalLight
{
    vec3 direction;
    vec3 color;
    float intensity;
    int shadowLayer;
};

struct PointLight
{
    vec3 position;
    vec3 color;
    float intensity;

    float constant;
    float linear;
    float quadratic;

    int shadowLayer;
    float farPlane;
};

struct SpotLight
{
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;

    float range;
    float innerCutoff;
    float outerCutoff;

    mat4 lightSpaceMatrix;
    int shadowLayer;
};

#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 10
#define CASCADE_COUNT 4

uniform DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];

uniform int directionalLightCount;
uniform int pointLightCount;
uniform int spotLightCount;

uniform mat4 csmView;
uniform float cascadeSplits[CASCADE_COUNT];
uniform mat4 cascadeLightSpaceMatrices[
    MAX_DIRECTIONAL_LIGHTS * CASCADE_COUNT
];

const float PI = 3.14159265359;

float calculateDirectionalShadow(
    DirectionalLight light,
    vec3 fragPos,
    vec3 normal,
    vec3 lightDir)
{
    vec3 positionDx = dFdx(fragPos);
    vec3 positionDy = dFdy(fragPos);

    if (light.shadowLayer < 0)
        return 0.0;

    float viewDepth = -(csmView * vec4(fragPos, 1.0)).z;

    if (viewDepth > cascadeSplits[CASCADE_COUNT - 1])
        return 0.0;

    int cascade = 0;

    for (int i = 0; i < CASCADE_COUNT - 1; ++i)
    {
        if (viewDepth > cascadeSplits[i])
            ++cascade;
    }

    int layer = light.shadowLayer * CASCADE_COUNT + cascade;
    mat4 lightMatrix = cascadeLightSpaceMatrices[layer];

    vec4 fragPosLightSpace = lightMatrix * vec4(fragPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (any(lessThan(projCoords, vec3(0.0))) ||
        any(greaterThan(projCoords, vec3(1.0))))
    {
        return 0.0;
    }

    vec3 shadowDx = mat3(lightMatrix) * positionDx * 0.5;
    vec3 shadowDy = mat3(lightMatrix) * positionDy * 0.5;

    float determinant =
        shadowDx.x * shadowDy.y - shadowDx.y * shadowDy.x;

    vec2 depthGradient = vec2(0.0);

    if (abs(determinant) > 1e-12)
    {
        depthGradient = vec2(
            shadowDy.y * shadowDx.z - shadowDx.y * shadowDy.z,
            shadowDx.x * shadowDy.z - shadowDy.x * shadowDx.z
        ) / determinant;
    }

    float bias = max(
        0.0005 * (1.0 - dot(normal, lightDir)),
        0.00005
    );

    vec2 mapSize = vec2(textureSize(directionalShadowMaps, 0).xy);
    vec2 texelPosition = projCoords.xy * mapSize - 0.5;
    vec2 baseTexel = floor(texelPosition);
    vec2 fraction = fract(texelPosition);

    float shadow = 0.0;

    for (int x = -1; x <= 2; ++x)
    {
        float weightX = x == -1
            ? 1.0 - fraction.x
            : (x == 2 ? fraction.x : 1.0);

        for (int y = -1; y <= 2; ++y)
        {
            float weightY = y == -1
                ? 1.0 - fraction.y
                : (y == 2 ? fraction.y : 1.0);

            vec2 sampleUV =
                (baseTexel + vec2(x, y) + 0.5) / mapSize;

            float closestDepth = texture(
                directionalShadowMaps,
                vec3(sampleUV, float(layer))
            ).r;

            float receiverDepth = projCoords.z
                + dot(depthGradient, sampleUV - projCoords.xy);

            if (receiverDepth - bias > closestDepth)
                shadow += weightX * weightY;
        }
    }

    float distanceFade = 1.0 - smoothstep(
        cascadeSplits[CASCADE_COUNT - 1] * 0.9,
        cascadeSplits[CASCADE_COUNT - 1],
        viewDepth
    );

    return shadow / 9.0 * distanceFade;
}

float calculateSpotShadow(
    SpotLight light,
    vec3 fragPos,
    vec3 normal,
    vec3 lightDir)
{
    if (light.shadowLayer < 0)
        return 0.0;

    vec4 fragPosLightSpace =
        light.lightSpaceMatrix * vec4(fragPos, 1.0);

    if (fragPosLightSpace.w <= 0.0)
        return 0.0;

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (any(lessThan(projCoords, vec3(0.0))) ||
        any(greaterThan(projCoords, vec3(1.0))))
    {
        return 0.0;
    }

    float bias = max(
        0.005 * (1.0 - dot(normal, lightDir)),
        0.0005
    );

    vec2 texelSize =
        1.0 / vec2(textureSize(spotShadowMaps, 0).xy);

    float shadow = 0.0;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec2 uv = projCoords.xy + vec2(x, y) * texelSize;

            float closestDepth = texture(
                spotShadowMaps,
                vec3(uv, float(light.shadowLayer))
            ).r;

            if (projCoords.z - bias > closestDepth)
                shadow += 1.0;
        }
    }

    return shadow / 9.0;
}

const vec3 pointShadowSampleOffsets[20] = vec3[](
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0, -1.0,  1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),

    vec3( 1.0,  1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),

    vec3( 1.0,  1.0,  0.0),
    vec3( 1.0, -1.0,  0.0),
    vec3(-1.0, -1.0,  0.0),
    vec3(-1.0,  1.0,  0.0),

    vec3( 1.0,  0.0,  1.0),
    vec3(-1.0,  0.0,  1.0),
    vec3( 1.0,  0.0, -1.0),
    vec3(-1.0,  0.0, -1.0),

    vec3( 0.0,  1.0,  1.0),
    vec3( 0.0, -1.0,  1.0),
    vec3( 0.0,  1.0, -1.0),
    vec3( 0.0, -1.0, -1.0)
);

float calculatePointShadow(PointLight light, vec3 fragPos)
{
    if (light.shadowLayer < 0 || light.farPlane <= 0.0)
        return 0.0;

    vec3 fragToLight = fragPos - light.position;
    float currentDepth = length(fragToLight);

    float bias = 0.05;
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + viewDistance / light.farPlane) / 25.0;

    float shadow = 0.0;

    for (int i = 0; i < 20; ++i)
    {
        float closestDepth = texture(
            pointShadowMaps,
            vec4(
                fragToLight + pointShadowSampleOffsets[i] * diskRadius,
                float(light.shadowLayer)
            )
        ).r;

        closestDepth *= light.farPlane;

        if (currentDepth - bias > closestDepth)
            shadow += 1.0;
    }

    return shadow / 20.0;
}


vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (vec3(1.0) - F0)
        * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(
    float cosTheta,
    vec3 F0,
    float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
        * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float denominator = NdotH * NdotH * (a2 - 1.0) + 1.0;
    denominator = PI * denominator * denominator;

    return a2 / denominator;
}

float geometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;

    return NdotX / (NdotX * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    return geometrySchlickGGX(NdotV, roughness)
        * geometrySchlickGGX(NdotL, roughness);
}

float calculateSpotFactor(
    vec3 L,
    vec3 direction,
    float innerCutoff,
    float outerCutoff)
{
    float theta = dot(-L, normalize(direction));

    if (innerCutoff <= outerCutoff)
        return step(outerCutoff, theta);

    return smoothstep(outerCutoff, innerCutoff, theta);
}

vec3 evaluateDirectBRDF(
    vec3 N,
    vec3 V,
    vec3 L,
    vec3 albedo,
    float metallic,
    float roughness,
    vec3 F0)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    if (NdotL <= 0.0)
        return vec3(0.0);

    vec3 halfVector = V + L;

    if (dot(halfVector, halfVector) < 1e-10)
        return vec3(0.0);

    vec3 H = normalize(halfVector);

    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    vec3 specular =
        D * F * G / (4.0 * NdotV * NdotL + 0.0001);

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * NdotL;
}

vec3 fallbackTangent(vec3 N)
{
    vec3 helper = abs(N.z) < 0.999
        ? vec3(0.0, 0.0, 1.0)
        : vec3(0.0, 1.0, 0.0);

    return normalize(cross(helper, N));
}

vec3 readWorldNormal()
{
    vec3 N = normalize(TBN[2]);
    vec3 T = TBN[0] - N * dot(N, TBN[0]);

    if (dot(T, T) > 1e-10)
        T = normalize(T);
    else
        T = fallbackTangent(N);

    float handedness =
        dot(cross(N, T), TBN[1]) < 0.0 ? -1.0 : 1.0;

    vec3 B = handedness * normalize(cross(N, T));

    vec2 normalXY =
        texture(normalMap, TexCoord).rg * 2.0 - 1.0;

    normalXY.y *= normalYSign;

    float normalZ =
        sqrt(max(1.0 - dot(normalXY, normalXY), 0.0));

    vec3 tangentNormal = normalize(vec3(normalXY, normalZ));
    vec3 normal = normalize(mat3(T, B, N) * tangentNormal);

    if (doubleSided && !gl_FrontFacing)
        normal = -normal;

    return normal;
}

void main()
{
    vec4 baseColor =
        texture(albedoMap, TexCoord) * baseColorFactor;

    float alpha = clamp(baseColor.a, 0.0, 1.0);
    vec3 albedo = baseColor.rgb;

    vec3 N = readWorldNormal();
    vec3 V = normalize(viewPos - FragPos);

    float metallic = clamp(
        texture(metallicMap, TexCoord).b * metallicFactor,
        0.0,
        1.0
    );

    float roughness = clamp(
        texture(roughnessMap, TexCoord).g * roughnessFactor,
        0.045,
        1.0
    );

    float ao = clamp(
        texture(aoMap, TexCoord).r,
        0.0,
        1.0
    );

    vec3 emissiveTexel = hasEmissiveMap
        ? texture(emissiveMap, TexCoord).rgb
        : vec3(1.0);

    vec3 emission =
        emissiveTexel * emissiveFactor * emissiveStrength;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NdotV = max(dot(N, V), 0.0);

    vec3 FIBL = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kDIBL = (vec3(1.0) - FIBL) * (1.0 - metallic);

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;

    vec3 R = reflect(-V, N);
    const float MAX_REFLECTION_LOD = 4.0;

    vec3 prefilteredColor = textureLod(
        prefilterMap,
        R,
        roughness * MAX_REFLECTION_LOD
    ).rgb;

    vec2 brdf = texture(
        brdfLUT,
        vec2(NdotV, roughness)
    ).rg;

    vec3 specularIBL =
        prefilteredColor * (FIBL * brdf.x + brdf.y);

    vec3 lighting =
        (kDIBL * diffuseIBL + specularIBL) * ao;

    for (int i = 0;
         i < min(directionalLightCount, MAX_DIRECTIONAL_LIGHTS);
         ++i)
    {
        DirectionalLight light = directionalLights[i];
        vec3 L = normalize(-light.direction);

        float shadow =
            calculateDirectionalShadow(light, FragPos, N, L);

        vec3 radiance = light.color * light.intensity;

        vec3 contribution = evaluateDirectBRDF(
            N, V, L, albedo, metallic, roughness, F0
        );

        lighting +=
            (1.0 - shadow) * contribution * radiance;
    }

    for (int i = 0;
         i < min(pointLightCount, MAX_POINT_LIGHTS);
         ++i)
    {
        PointLight light = pointLights[i];

        vec3 lightVector = light.position - FragPos;
        float distanceToLight = length(lightVector);

        if (distanceToLight < 0.0001 ||
            distanceToLight >= light.farPlane)
        {
            continue;
        }

        vec3 L = lightVector / distanceToLight;

        float attenuation = 1.0 / max(
            light.constant +
            light.linear * distanceToLight +
            light.quadratic * distanceToLight * distanceToLight,
            0.0001
        );

        vec3 radiance =
            light.color * light.intensity * attenuation;

        float shadow = calculatePointShadow(light, FragPos);

        vec3 contribution = evaluateDirectBRDF(
            N, V, L, albedo, metallic, roughness, F0
        );

        lighting +=
            (1.0 - shadow) * contribution * radiance;
    }

    for (int i = 0;
         i < min(spotLightCount, MAX_SPOT_LIGHTS);
         ++i)
    {
        SpotLight light = spotLights[i];

        vec3 lightVector = light.position - FragPos;
        float distanceToLight = length(lightVector);

        if (distanceToLight < 0.0001 ||
            distanceToLight >= light.range)
        {
            continue;
        }

        vec3 L = lightVector / distanceToLight;

        float spotFactor = calculateSpotFactor(
            L,
            light.direction,
            light.innerCutoff,
            light.outerCutoff
        );

        if (spotFactor <= 0.0)
            continue;

        float attenuation = 1.0 / (
            1.0 +
            0.09 * distanceToLight +
            0.032 * distanceToLight * distanceToLight
        );

        float rangeFactor = 1.0 - smoothstep(
            0.8 * light.range,
            light.range,
            distanceToLight
        );

        vec3 radiance = light.color * light.intensity
            * attenuation * rangeFactor * spotFactor;

        float shadow =
            calculateSpotShadow(light, FragPos, N, L);

        vec3 contribution = evaluateDirectBRDF(
            N, V, L, albedo, metallic, roughness, F0
        );

        lighting +=
            (1.0 - shadow) * contribution * radiance;
    }

    if (alpha <= 0.0)
        discard;

    FragColor = vec4(lighting + emission, alpha);
}