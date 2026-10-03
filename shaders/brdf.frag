#version 450 core

layout(location = 0) out vec2 FragColor;

in vec2 TexCoord;

const float PI = 3.14159265359;

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint count)
{
    return vec2(float(i) / float(count), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;

    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));

    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));

    vec3 H;

    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 helper = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);

    vec3 tangent = normalize(cross(helper, N));

    vec3 bitangent = cross(N, tangent);
    
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) * 0.5;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);

    float NdotL = max(dot(N, L), 0.0);

    float ggxV = GeometrySchlickGGX(NdotV, roughness);

    float ggxL = GeometrySchlickGGX(NdotL, roughness);

    return ggxV * ggxL;
}

vec2 IntegrateBRDF(float NdotV, float roughness)
{
    vec3 V;
    
    V.x = sqrt(max(0.0, 1.0 - NdotV * NdotV));

    V.y = 0.0;

    V.z = NdotV;

    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;

    const uint sampleCount = 1024u;

    for(uint i =0u; i < sampleCount; ++i)
    {
        vec2 Xi = Hammersley(i, sampleCount);

        vec3 H = ImportanceSampleGGX(Xi, N, roughness);

        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);

        float NdotH = max(dot(N, H), 0.0);

        float VdotH = max(dot(V, H), 0.0);

        if(NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness);

            float GVis = (G* VdotH) / max(NdotH * NdotV, 0.001);

            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * GVis;

            B += Fc * GVis;
        }
    }

    A /= float(sampleCount);
    B /= float(sampleCount);

    return vec2(A, B);
}

void main()
{
    FragColor = IntegrateBRDF(TexCoord.x, TexCoord.y);
}