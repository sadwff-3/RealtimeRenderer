#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos =
        model * vec4(aPos, 1.0);

    FragPos = worldPos.xyz;
    TexCoord = aTexCoord;

    // N：模型局部法线 → 世界空间
    vec3 N = normalize(
        mat3(transpose(inverse(model))) * aNormal
    );

    // T：模型局部 Tangent → 世界空间
    vec3 T = normalize(
        mat3(model) * aTangent
    );

    // B：模型局部 Bitangent → 世界空间
    vec3 B = normalize(
        mat3(model) * aBitangent
    );

    Normal = N;

    TBN = mat3(T, B, N);

    gl_Position =
        projection * view * worldPos;
}