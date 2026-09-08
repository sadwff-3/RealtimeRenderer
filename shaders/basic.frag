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

uniform float ambientStrength;
uniform float specularStrength;
uniform float shininess;

float calculateShadow(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.005;
    float shadow = currentDepth - bias  > closestDepth ? 1.0 : 0.0;

    return shadow;
}

void main()
{
    vec3 textureColor = texture(texture1, TexCoord).rgb;

    vec3 ambient = ambientStrength * textureColor;

    vec3 tangentNormal = texture(normalMap, TexCoord).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;

    vec3 norm = normalize(TBN * tangentNormal);

    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 lighting = ambient;

    vec3 dirLightDir = normalize(-directionalLight.direction);

float dirDiff = max(dot(norm, dirLightDir), 0.0);

vec3 dirDiffuse = dirDiff
    * directionalLight.color
    * directionalLight.intensity
    * textureColor;

vec3 dirHalfwayDir = normalize(dirLightDir + viewDir);

float dirSpec = pow(max(dot(norm, dirHalfwayDir), 0.0), shininess);

vec3 dirSpecular = specularStrength
    * dirSpec
    * directionalLight.color
    * directionalLight.intensity;

float shadow = calculateShadow(FragPosLightSpace);
lighting += (1.0 - shadow) * (dirDiffuse + dirSpecular);

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

        float diff = max(dot(norm, lightDir), 0.0);

        vec3 diffuse =
            diff
            * pointLights[i].color
            * pointLights[i].intensity
            * attenuation
            * textureColor;

        vec3 halfwayDir = normalize(lightDir + viewDir);

        float spec =
            pow(max(dot(norm, halfwayDir), 0.0), shininess);

        vec3 specular =
            specularStrength
            * spec
            * pointLights[i].color
            * pointLights[i].intensity
            *attenuation;

        lighting += diffuse + specular;
    }

    FragColor = vec4(lighting, 1.0);
}