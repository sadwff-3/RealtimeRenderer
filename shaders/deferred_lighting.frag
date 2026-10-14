#version 450 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMaterial;
uniform sampler2D gDepth;
uniform sampler2D gVelocity;
uniform sampler2D gEmission;
uniform sampler2D ssaoMap;
uniform sampler2DArray directionalShadowMaps;
uniform sampler2DArray spotShadowMaps;
uniform samplerCubeArray pointShadowMaps;
uniform samplerCube environmentMap;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

uniform mat4 inverseViewProjection;
uniform vec3 viewPos;
uniform int debugView;

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
uniform DirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS];
uniform int directionalLightCount;


#define CASCADE_COUNT 4
uniform mat4 csmView;
uniform float cascadeSplits[CASCADE_COUNT];
uniform mat4 cascadeLightSpaceMatrices[MAX_DIRECTIONAL_LIGHTS * CASCADE_COUNT];


#define MAX_POINT_LIGHTS 4
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int pointLightCount;


#define MAX_SPOT_LIGHTS 4
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];
uniform int spotLightCount;


uniform DirectionalLight directionalLight;

const float PI = 3.14159265359;

float calculateDirectionalShadow(DirectionalLight light, vec3 fragPos, vec3 normal, vec3 lightDir)
{
    // Take derivatives before cascade selection or fragment-dependent returns.
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

    if (any(lessThan(projCoords, vec3(0.0))) || any(greaterThan(projCoords, vec3(1.0))))
        return 0.0;

    // Orthographic CSM: transform the receiver's surface derivatives to UV/depth.
    vec3 shadowDx = mat3(lightMatrix) * positionDx * 0.5;
    vec3 shadowDy = mat3(lightMatrix) * positionDy * 0.5;
    float determinant = shadowDx.x * shadowDy.y - shadowDx.y * shadowDy.x;
    vec2 depthGradient = vec2(0.0);
    if (abs(determinant) > 1e-12)
    {
        depthGradient = vec2(shadowDy.y * shadowDx.z - shadowDx.y * shadowDy.z, shadowDx.x * shadowDy.z - shadowDy.x * shadowDx.z) / determinant;
    }

    float bias = max(0.0005 * (1.0 - dot(normal, lightDir)), 0.00005);
    vec2 mapSize = vec2(textureSize(directionalShadowMaps, 0).xy);
    vec2 texelPosition = projCoords.xy * mapSize - 0.5;
    vec2 baseTexel = floor(texelPosition);
    vec2 fraction = fract(texelPosition);
    float shadow = 0.0;

    // The 16 unique taps equal a bilinearly filtered 3x3 comparison kernel.
    for (int x = -1; x <= 2; ++x)
    {
        float weightX = x == -1 ? 1.0 - fraction.x : (x == 2 ? fraction.x : 1.0);
        for (int y = -1; y <= 2; ++y)
        {
            float weightY = y == -1 ? 1.0 - fraction.y : (y == 2 ? fraction.y : 1.0);
            vec2 sampleUV = (baseTexel + vec2(x, y) + 0.5) / mapSize;
            float closestDepth = texture(directionalShadowMaps, vec3(sampleUV, float(layer))).r;
            float receiverDepth = projCoords.z + dot(depthGradient, sampleUV - projCoords.xy);
            if (receiverDepth - bias > closestDepth)
                shadow += weightX * weightY;
        }
    }

    float distanceFade = 1.0 - smoothstep(cascadeSplits[CASCADE_COUNT - 1] * 0.9, cascadeSplits[CASCADE_COUNT - 1], viewDepth);
    return shadow / 9.0 * distanceFade;
}


float calculateSpotShadow(SpotLight light, vec3 fragPos, vec3 normal, vec3 lightDir)
{
    if(light.shadowLayer < 0)
       return 0.0;
       
    vec4 fragPosLightSpace = light.lightSpaceMatrix * vec4(fragPos, 1.0);

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords* 0.5 + 0.5;

    if(any(lessThan(projCoords, vec3(0.0))) || any(greaterThan(projCoords, vec3(1.0))))
    {
        return 0.0;
    }

    float currentDepth = projCoords.z;

    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    vec2 texelSize = 1.0 / vec2(textureSize(spotShadowMaps, 0).xy);

    float shadow = 0.0;

    for(int x = -1; x<= 1; ++x)
    {
        for(int y = -1; y<= 1; ++y)
        {
            vec2 uv = projCoords.xy + vec2(x, y) * texelSize;
            float closestDepth = texture(spotShadowMaps, vec3(uv, float(light.shadowLayer))).r;

            if(currentDepth - bias > closestDepth)
               shadow += 1.0;
        }
    }

    return shadow / 9.0;
}

const vec3 pointShadowSampleOffsets[20] =
    {
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
    };

float calculatePointShadow(PointLight light, vec3 fragPos)
{

    if(light.shadowLayer < 0)
       return 0.0;
    
    vec3 fragToLight = fragPos - light.position;

    float currentDepth = length(fragToLight);

    float bias = 0.05;

    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + viewDistance / light.farPlane) / 25.0;

    float shadow = 0.0;

    for(int i = 0; i < 20; ++i)
    {
        float closestDepth = texture(pointShadowMaps, vec4(fragToLight + pointShadowSampleOffsets[i] * diskRadius, float(light.shadowLayer))).r;

        closestDepth *= light.farPlane;

        if(currentDepth - bias > closestDepth)
           shadow += 1.0;
    }

    return shadow / 20.0;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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

float calculateSpotFactor(vec3 L, vec3 direction, float innerCutoff, float outerCutoff)
{
    float theta = dot(-L, normalize(direction));

    return smoothstep(outerCutoff, innerCutoff, theta);
}

void main()
{
    float depth = texture(gDepth, TexCoord).r;

    if (debugView == 9)
    {
        FragColor = vec4(texture(gEmission, TexCoord).rgb, 1.0);
        return;
    }

    if (debugView == 8)
    {
        vec2 velocity = texture(gVelocity, TexCoord).rg;

        vec3 color = vec3(0.5 + velocity.x * 32.0, 0.5 + velocity.y * 32.0, 0.5);

        FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
        return;
    }

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
        vec2 ndc = TexCoord * 2.0 - 1.0;

        vec4 worldPos = inverseViewProjection * vec4(ndc, 1.0, 1.0);

        worldPos /= worldPos.w;

        vec3 viewDirection = normalize(worldPos.xyz - viewPos);

        vec3 environmentColor = texture(environmentMap, viewDirection).rgb;

        FragColor = vec4(environmentColor, 1.0);

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

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float ssao = texture(ssaoMap, TexCoord).r;

    float NdotV = max(dot(N, V), 0.0);

    vec3 FIBL = fresnelSchlickRoughness(NdotV, F0, roughness);

    vec3 kSIBL = FIBL;

    vec3 kDIBL = (vec3(1.0) - kSIBL) * (1.0 - metallic);

    vec3 irradiance = texture(irradianceMap, N).rgb;

    vec3 diffuseIBL = irradiance * albedo;

    vec3 R = reflect(-V, N);

    const float MAX_REFLECTION_LOD = 4.0;

    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;

    vec3 specularIBL = prefilteredColor * (FIBL * brdf.x + brdf.y);

    // Keep diffuse fill subdued while retaining more environment reflection on metal.
    vec3 ambient = (kDIBL * diffuseIBL + specularIBL) * ao * ssao;

    vec3 lighting = ambient;

    for(int i = 0; i < min(directionalLightCount, MAX_DIRECTIONAL_LIGHTS); ++i)
    {
        DirectionalLight light = directionalLights[i];

        vec3 L = normalize(-light.direction);
        vec3 H = normalize(V + L);

        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float D = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);

        vec3 specular =(D * F * G) / (4.0 * NdotV * NdotL + 0.0001);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        vec3 diffuse = kD * albedo / PI;

        vec3 radiance = light.color * light.intensity;
    
        float shadow = calculateDirectionalShadow(light, fragPos, N, L);

        lighting += (1.0 - shadow) * (diffuse + specular) * radiance * NdotL;
    }

    for(int i = 0; i < min(pointLightCount, MAX_POINT_LIGHTS); ++i)
    {
        vec3 lightVector = pointLights[i].position - fragPos;
        float distance = length(lightVector);

        if(distance < 0.0001 || distance >= pointLights[i].farPlane)
           continue;
        
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

        float shadow = calculatePointShadow(pointLights[i], fragPos);

        lighting += (1.0 - shadow) * (diffuse + specular) * radiance * NdotL;
    }

    for(int i = 0; i < min(spotLightCount, MAX_SPOT_LIGHTS); ++i)
    {
        SpotLight light = spotLights[i];

        vec3 lightVector = light.position - fragPos;
        float distance = length(lightVector);

        if(distance < 0.0001 || distance >= light.range)
           continue;
        
        vec3 L = lightVector / distance;
        float spotFactor = calculateSpotFactor(L, light.direction, light.innerCutoff, light.outerCutoff);
        
        if(spotFactor <= 0.0)
           continue;

        vec3 H = normalize(V + L);

        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
        float rangeFactor = 1.0 - smoothstep(0.8 * light.range, light.range, distance);

        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float D = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);

        vec3 specular = (D * F * G) / (4.0 * NdotV * NdotL + 0.0001);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        vec3 diffuse = kD * albedo / PI;

        vec3 radiance = light.color * light.intensity * attenuation * rangeFactor * spotFactor;
  
        float shadow = calculateSpotShadow(light, fragPos, N, L);
        lighting += (1.0 - shadow) * (diffuse + specular) * radiance * NdotL;

    }

    vec3 emission = texture(gEmission, TexCoord).rgb;
    FragColor = vec4(lighting + emission, 1.0);
}