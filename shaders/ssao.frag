#version 330 core

in vec2 TexCoord;
layout(location = 0) out float FragAO;

uniform sampler2D gDepth;
uniform sampler2D gNormal;

uniform mat4 inverseProjection;
uniform mat4 view;
uniform mat4 projection;
uniform sampler2D noiseTexture;
uniform vec3 samples[32];

vec3 reconstructionViewPosition(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);

    vec4 viewPos = inverseProjection * clipPos;

    return viewPos.xyz / viewPos.w;
}

void main()
{
    float depth = texture(gDepth, TexCoord).r;

    if (depth >= 1.0)
    {
        FragAO = 1.0;
        return;
    }

    vec3 fragPos = reconstructionViewPosition(TexCoord, depth);

    vec3 worldNormal = texture(gNormal, TexCoord).rgb;
    vec3 normal = normalize(mat3(view) * worldNormal);

    vec3 randomVec = normalize(texture(noiseTexture, gl_FragCoord.xy / 4.0).xyz);

    vec3 tangent = randomVec - normal * dot(randomVec, normal);
    tangent = normalize(tangent);
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    const float radius = 0.5;
    const float bias = 0.025;

    float occlusion = 0.0;

    for(int i = 0; i < 32; ++i)
    {
        vec3 samplePos = fragPos + TBN* samples[i] * radius;

        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        vec2 sampleUV = offset.xy * 0.5 + 0.5;

        if(any(lessThan(sampleUV, vec2(0.0))) || any(greaterThan(sampleUV, vec2(1.0))))continue;

        float sampleDepthRaw = texture(gDepth, sampleUV).r;
        if (sampleDepthRaw >= 1.0)continue;

        float sampleDepth = reconstructionViewPosition(sampleUV, sampleDepthRaw).z;

        float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(fragPos.z - sampleDepth), 0.0001));

        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    FragAO = 1.0 - occlusion / 32.0;
}